/*
 * ============================================================
 *  PENDULUM COLLISION SIMULATION
 *  Full manual rasterization + physics — C++ / SFML 2.6.1
 * ============================================================
 *  Rasterization (zero built-in math):
 *      - Bresenham line algorithm          (strings, bars, grid)
 *      - Midpoint circle algorithm         (outlines, 8 octants)
 *      - Scanline circle fill              (per-pixel shading)
 *
 *  Per-pixel shading (inside scanline fill):
 *      - Lambert diffuse lighting
 *      - Phong specular highlights
 *      - Fresnel rim glow
 *      - Radial outer glow ring
 *
 *  Physics:
 *      - Pendulum torque:  a = -(g / L) * sin(theta)
 *      - Elastic collision detection + impulse resolution
 *      - Sub-stepping (8 steps/frame) for stability
 *
 *  Build (Visual Studio):
 *      Add SFML include/lib paths in project properties.
 *      Link: sfml-graphics.lib  sfml-window.lib  sfml-system.lib
 *      Compile as C++17.
 *
 *  Build (g++ command line):
 *      g++ -O2 -std=c++17 BallCollision.cpp -o BallCollision
 *          -I<sfml-include> -L<sfml-lib>
 *          -lsfml-graphics -lsfml-window -lsfml-system
 *
 *  Controls:
 *      Left click anywhere  — reset simulation
 *      Close window         — quit
 * ============================================================
 */

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <cmath>
#include <algorithm>
#include <cstdio>

 // ════════════════════════════════════════════════════════════════════════════
 //  CONSTANTS
 // ════════════════════════════════════════════════════════════════════════════
static const unsigned int WIN_W = 900u;
static const unsigned int WIN_H = 700u;
static const float        GRAVITY = 980.0f;
static const float        DAMPING = 0.9996f;
static const int          SUB_STEPS = 8;
static const int          BALL_RADIUS = 28;

// ════════════════════════════════════════════════════════════════════════════
//  PIXEL BUFFER
//  Raw RGBA array.  Every pixel is written manually.
//  Blit path: pixelBuf -> sf::Image::create -> sf::Texture -> draw
// ════════════════════════════════════════════════════════════════════════════
static sf::Uint8 pixelBuf[WIN_W * WIN_H * 4];

static void clearBuffer(sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
    unsigned int total = WIN_W * WIN_H;
    for (unsigned int i = 0; i < total; ++i)
    {
        unsigned int base = i * 4u;
        pixelBuf[base + 0] = r;
        pixelBuf[base + 1] = g;
        pixelBuf[base + 2] = b;
        pixelBuf[base + 3] = 255;
    }
}

// Alpha-blend onto existing pixel
static void setPixel(int x, int y,
    sf::Uint8 r, sf::Uint8 g, sf::Uint8 b, sf::Uint8 a = 255)
{
    if (x < 0 || y < 0 || x >= (int)WIN_W || y >= (int)WIN_H)
        return;

    unsigned int idx = ((unsigned int)y * WIN_W + (unsigned int)x) * 4u;

    if (a == 255)
    {
        pixelBuf[idx + 0] = r;
        pixelBuf[idx + 1] = g;
        pixelBuf[idx + 2] = b;
        pixelBuf[idx + 3] = 255;
    }
    else
    {
        float fa = (float)a / 255.0f;
        float fb = ((float)pixelBuf[idx + 3] / 255.0f) * (1.0f - fa);

        pixelBuf[idx + 0] = (sf::Uint8)((float)r * fa + (float)pixelBuf[idx + 0] * fb);
        pixelBuf[idx + 1] = (sf::Uint8)((float)g * fa + (float)pixelBuf[idx + 1] * fb);
        pixelBuf[idx + 2] = (sf::Uint8)((float)b * fa + (float)pixelBuf[idx + 2] * fb);
        pixelBuf[idx + 3] = (sf::Uint8)((fa + fb) * 255.0f);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  BRESENHAM LINE
// ════════════════════════════════════════════════════════════════════════════
static void bresenhamLine(int x0, int y0, int x1, int y1,
    sf::Uint8 r, sf::Uint8 g, sf::Uint8 b, sf::Uint8 a = 255)
{
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        setPixel(x0, y0, r, g, b, a);
        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Thick line: N offset copies along the perpendicular
static void thickLine(int x0, int y0, int x1, int y1, int thickness,
    sf::Uint8 r, sf::Uint8 g, sf::Uint8 b, sf::Uint8 a = 255)
{
    float dx = (float)(x1 - x0);
    float dy = (float)(y1 - y0);
    float len = (float)std::sqrt((double)(dx * dx + dy * dy));

    if (len < 0.001f)
    {
        setPixel(x0, y0, r, g, b, a);
        return;
    }

    // Perpendicular unit vector
    float px = -dy / len;
    float py = dx / len;

    float half = (float)(thickness - 1) * 0.5f;
    for (float i = -half; i <= half; i += 1.0f)
    {
        bresenhamLine((int)((float)x0 + px * i), (int)((float)y0 + py * i),
            (int)((float)x1 + px * i), (int)((float)y1 + py * i),
            r, g, b, a);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  MIDPOINT CIRCLE  (outline — all 8 octants)
// ════════════════════════════════════════════════════════════════════════════
static void midpointCircle(int cx, int cy, int radius,
    sf::Uint8 r, sf::Uint8 g, sf::Uint8 b, sf::Uint8 a = 255)
{
    int x = radius;
    int y = 0;
    int p = 1 - radius;

    while (x >= y)
    {
        setPixel(cx + x, cy + y, r, g, b, a);
        setPixel(cx - x, cy + y, r, g, b, a);
        setPixel(cx + x, cy - y, r, g, b, a);
        setPixel(cx - x, cy - y, r, g, b, a);
        setPixel(cx + y, cy + x, r, g, b, a);
        setPixel(cx - y, cy + x, r, g, b, a);
        setPixel(cx + y, cy - x, r, g, b, a);
        setPixel(cx - y, cy - x, r, g, b, a);

        ++y;
        if (p < 0)
        {
            p += 2 * y + 1;
        }
        else
        {
            --x;
            p += 2 * (y - x) + 1;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  SCANLINE FILL  —  circle with per-pixel Lambert + Phong + Fresnel shading
// ════════════════════════════════════════════════════════════════════════════

// Light direction (top-left).  Pre-normalised.
static const float LIGHT_LX = -0.5f / 0.8602f;   // -0.5 / sqrt(0.25+0.49)
static const float LIGHT_LY = -0.7f / 0.8602f;

static void scanlineFillCircle(int cx, int cy, int radius,
    sf::Uint8 baseR, sf::Uint8 baseG, sf::Uint8 baseB,
    sf::Uint8 glowR, sf::Uint8 glowG, sf::Uint8 glowB)
{
    float invR = 1.0f / (float)radius;

    for (int y = cy - radius; y <= cy + radius; ++y)
    {
        float dy = (float)(y - cy);
        float disc = (float)(radius * radius) - dy * dy;
        if (disc < 0.0f) continue;

        float sqD = (float)std::sqrt((double)disc);
        int   xLeft = (int)((float)cx - sqD);
        int   xRight = (int)((float)cx + sqD);

        for (int x = xLeft; x <= xRight; ++x)
        {
            float dx = (float)(x - cx);

            // Distance from centre, normalised [0 … 1]
            float dist = (float)std::sqrt((double)(dx * dx + dy * dy)) * invR;

            // ── Sphere normal (unit sphere) ──
            float nx = dx * invR;
            float ny = dy * invR;
            float nz2 = 1.0f - nx * nx - ny * ny;
            float nz = (nz2 > 0.0f) ? (float)std::sqrt((double)nz2) : 0.0f;

            // ── Lambert diffuse ──
            float diff = nx * LIGHT_LX + ny * LIGHT_LY + nz * 0.7f;
            if (diff < 0.0f) diff = 0.0f;

            // ── Phong specular  (view along +Z) ──
            float spec = nz * 0.9f + diff * 0.3f;
            if (spec < 0.0f) spec = 0.0f;
            spec = spec * spec;   // ^2
            spec = spec * spec;   // ^4
            spec = spec * spec;   // ^8

            // ── Combine ──
            float shade = 0.12f + diff * 0.70f + spec * 0.50f;
            if (shade > 1.4f) shade = 1.4f;

            float fR = (float)baseR * shade;
            float fG = (float)baseG * shade;
            float fB = (float)baseB * shade;

            // ── Fresnel rim ──
            float rim = dist * dist;
            fR += (float)glowR * rim * 0.30f;
            fG += (float)glowG * rim * 0.30f;
            fB += (float)glowB * rim * 0.30f;

            sf::Uint8 outR = (sf::Uint8)std::min(fR, 255.0f);
            sf::Uint8 outG = (sf::Uint8)std::min(fG, 255.0f);
            sf::Uint8 outB = (sf::Uint8)std::min(fB, 255.0f);

            setPixel(x, y, outR, outG, outB, 255);
        }
    }
}

// Outer glow ring  —  scanline filled, alpha fades to 0 at edge
static void scanlineGlowRing(int cx, int cy, int radius,
    sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
    float invR = 1.0f / (float)radius;

    for (int y = cy - radius; y <= cy + radius; ++y)
    {
        float dy = (float)(y - cy);
        float disc = (float)(radius * radius) - dy * dy;
        if (disc < 0.0f) continue;

        float sqD = (float)std::sqrt((double)disc);
        int   xLeft = (int)((float)cx - sqD);
        int   xRight = (int)((float)cx + sqD);

        for (int x = xLeft; x <= xRight; ++x)
        {
            float dx = (float)(x - cx);
            float dist = (float)std::sqrt((double)(dx * dx + dy * dy)) * invR;

            float alpha = (1.0f - dist) * 70.0f;
            if (alpha < 1.0f) continue;

            setPixel(x, y, r, g, b, (sf::Uint8)alpha);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  GRID
// ════════════════════════════════════════════════════════════════════════════
static void drawGrid()
{
    const int STEP = 60;

    // Vertical lines
    for (int x = 0; x < (int)WIN_W; x += STEP)
        for (int y = 0; y < (int)WIN_H; ++y)
            setPixel(x, y, 20, 20, 30, 35);

    // Horizontal lines
    for (int y = 0; y < (int)WIN_H; y += STEP)
        for (int x = 0; x < (int)WIN_W; ++x)
            setPixel(x, y, 20, 20, 30, 35);
}

// ════════════════════════════════════════════════════════════════════════════
//  BALL STRUCT
// ════════════════════════════════════════════════════════════════════════════
struct Ball
{
    float     pivotX, pivotY;
    float     length;
    float     angle;          // radians,  0 = straight down
    float     angularVel;
    int       radius;
    float     x, y;           // tip position (world)

    sf::Uint8 baseR, baseG, baseB;
    sf::Uint8 glowR, glowG, glowB;

    void updatePosition()
    {
        x = pivotX + length * (float)std::sin((double)angle);
        y = pivotY + length * (float)std::cos((double)angle);
    }
};

// ════════════════════════════════════════════════════════════════════════════
//  GLOBALS
// ════════════════════════════════════════════════════════════════════════════
static Ball  balls[2];
static float globalPivotY;

static void resetSimulation()
{
    globalPivotY = 80.0f;
    float cx = (float)WIN_W * 0.5f;

    // ── Ball A  (left, ember) ──
    balls[0].pivotX = cx - 60.0f;
    balls[0].pivotY = globalPivotY;
    balls[0].length = 220.0f;
    balls[0].angle = -0.65f;
    balls[0].angularVel = 0.0f;
    balls[0].radius = BALL_RADIUS;
    balls[0].baseR = 220; balls[0].baseG = 80; balls[0].baseB = 50;
    balls[0].glowR = 255; balls[0].glowG = 120; balls[0].glowB = 60;
    balls[0].updatePosition();

    // ── Ball B  (right, ice) ──
    balls[1].pivotX = cx + 60.0f;
    balls[1].pivotY = globalPivotY;
    balls[1].length = 240.0f;
    balls[1].angle = 0.60f;
    balls[1].angularVel = 0.0f;
    balls[1].radius = BALL_RADIUS;
    balls[1].baseR = 50; balls[1].baseG = 130; balls[1].baseB = 220;
    balls[1].glowR = 80; balls[1].glowG = 180; balls[1].glowB = 255;
    balls[1].updatePosition();
}

// ════════════════════════════════════════════════════════════════════════════
//  PHYSICS
// ════════════════════════════════════════════════════════════════════════════
static void physicsTick(float dt)
{
    // ── 1) Pendulum integration ──
    for (int i = 0; i < 2; ++i)
    {
        Ball& b = balls[i];
        float alpha = -(GRAVITY / b.length) * (float)std::sin((double)b.angle);
        b.angularVel += alpha * dt;
        b.angularVel *= DAMPING;
        b.angle += b.angularVel * dt;
        b.updatePosition();
    }

    // ── 2) Elastic collision ──
    Ball& A = balls[0];
    Ball& B = balls[1];

    float dx = B.x - A.x;
    float dy = B.y - A.y;
    float dist = (float)std::sqrt((double)(dx * dx + dy * dy));
    float minD = (float)(A.radius + B.radius);

    if (dist < minD && dist > 0.001f)
    {
        // Unit normal  A -> B
        float nx = dx / dist;
        float ny = dy / dist;

        // Tip velocities  (v = omega x r)
        float vAx = A.angularVel * A.length * (float)std::cos((double)A.angle);
        float vAy = -A.angularVel * A.length * (float)std::sin((double)A.angle);
        float vBx = B.angularVel * B.length * (float)std::cos((double)B.angle);
        float vBy = -B.angularVel * B.length * (float)std::sin((double)B.angle);

        // Relative velocity along normal
        float relVn = (vAx - vBx) * nx + (vAy - vBy) * ny;

        if (relVn > 0.0f)   // balls approaching each other
        {
            float j = relVn;   // impulse magnitude (equal mass)

            // Tangent directions of each pendulum
            float tAx = (float)std::cos((double)A.angle);
            float tAy = -(float)std::sin((double)A.angle);
            float tBx = (float)std::cos((double)B.angle);
            float tBy = -(float)std::sin((double)B.angle);

            // Angular velocity deltas
            float dOmA = -(j * (nx * tAx + ny * tAy)) / A.length;
            float dOmB = (j * (nx * tBx + ny * tBy)) / B.length;

            A.angularVel += dOmA;
            B.angularVel += dOmB;

            // Positional correction — separate overlapping balls
            float overlap = minD - dist;

            float cosA = (float)std::cos((double)A.angle);
            if (std::abs(cosA) < 0.01f) cosA = (cosA >= 0.0f) ? 0.01f : -0.01f;

            float cosB = (float)std::cos((double)B.angle);
            if (std::abs(cosB) < 0.01f) cosB = (cosB >= 0.0f) ? 0.01f : -0.01f;

            A.angle -= (overlap * 0.5f * nx) / (A.length * cosA);
            B.angle += (overlap * 0.5f * nx) / (B.length * cosB);

            A.updatePosition();
            B.updatePosition();
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  DRAW BALL   (glow ring + scanline body + midpoint outline + highlight)
// ════════════════════════════════════════════════════════════════════════════
static void drawBall(const Ball& b)
{
    int cx = (int)b.x;
    int cy = (int)b.y;

    scanlineGlowRing(cx, cy, b.radius + 6,
        b.glowR, b.glowG, b.glowB);

    scanlineFillCircle(cx, cy, b.radius,
        b.baseR, b.baseG, b.baseB,
        b.glowR, b.glowG, b.glowB);

    midpointCircle(cx, cy, b.radius,
        b.glowR, b.glowG, b.glowB, 180);

    // Highlight ring (offset toward light source)
    midpointCircle(cx - 2, cy - 2, (int)((float)b.radius * 0.55f),
        255, 255, 255, 50);
}

// ════════════════════════════════════════════════════════════════════════════
//  DRAW PIVOTS
// ════════════════════════════════════════════════════════════════════════════
static void drawPivots()
{
    for (int i = 0; i < 2; ++i)
    {
        int px = (int)balls[i].pivotX;
        int py = (int)balls[i].pivotY;

        scanlineGlowRing(px, py, 3, 100, 100, 120);
        midpointCircle(px, py, 4, 80, 80, 100);
    }

    // Horizontal bar
    int barL = (int)balls[0].pivotX - 20;
    int barR = (int)balls[1].pivotX + 20;
    thickLine(barL, (int)globalPivotY,
        barR, (int)globalPivotY, 3, 60, 60, 75);
}

// ════════════════════════════════════════════════════════════════════════════
//  DRAW STRINGS
// ════════════════════════════════════════════════════════════════════════════
static void drawStrings()
{
    for (int i = 0; i < 2; ++i)
    {
        thickLine((int)balls[i].pivotX, (int)balls[i].pivotY,
            (int)balls[i].x, (int)balls[i].y,
            2, 100, 100, 110, 200);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  HUD   (drawn via SFML Text — it is the UI layer, not part of the sim)
// ════════════════════════════════════════════════════════════════════════════
static void drawHUD(sf::RenderWindow& window)
{
    const float RAD2DEG = 180.0f / 3.14159265358979f;

    // ── Ball A ──
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "A   theta: %.1f deg   omega: %.3f",
            balls[0].angle * RAD2DEG, balls[0].angularVel);
        sf::Text t;
        t.setString(buf);
        t.setCharacterSize(13);
        t.setFillColor(sf::Color(192, 82, 44));
        t.setPosition(14.0f, (float)(WIN_H - 38));
        window.draw(t);
    }

    // ── Ball B ──
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "B   theta: %.1f deg   omega: %.3f",
            balls[1].angle * RAD2DEG, balls[1].angularVel);
        sf::Text t;
        t.setString(buf);
        t.setCharacterSize(13);
        t.setFillColor(sf::Color(44, 122, 192));
        t.setPosition(14.0f, (float)(WIN_H - 18));
        window.draw(t);
    }

    // ── Collision flash ──
    {
        float dx = balls[1].x - balls[0].x;
        float dy = balls[1].y - balls[0].y;
        float dist = (float)std::sqrt((double)(dx * dx + dy * dy));

        if (dist < (float)(balls[0].radius + balls[1].radius + 5))
        {
            sf::Text t;
            t.setString("! COLLISION");
            t.setCharacterSize(14);
            t.setStyle(sf::Text::Bold);
            t.setFillColor(sf::Color(255, 100, 80));
            t.setPosition((float)WIN_W - 130.0f, 18.0f);
            window.draw(t);
        }
    }

    // ── Title ──
    {
        sf::Text t;
        t.setString("PENDULUM COLLISION SIMULATION");
        t.setCharacterSize(11);
        t.setFillColor(sf::Color(70, 70, 90));
        t.setPosition((float)WIN_W * 0.5f - 140.0f, 10.0f);
        window.draw(t);
    }

    // ── Sub-title ──
    {
        sf::Text t;
        t.setString("Bresenham  |  Midpoint Circle  |  Scanline Fill  |  Manual Physics");
        t.setCharacterSize(10);
        t.setFillColor(sf::Color(55, 55, 70));
        t.setPosition((float)WIN_W * 0.5f - 195.0f, 24.0f);
        window.draw(t);
    }

    // ── Footer ──
    {
        sf::Text t;
        t.setString("Click to reset");
        t.setCharacterSize(10);
        t.setFillColor(sf::Color(40, 40, 55));
        t.setPosition(14.0f, (float)(WIN_H - 56));
        window.draw(t);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════════════════════
int main()
{
    sf::VideoMode   mode(WIN_W, WIN_H);
    sf::RenderWindow window(mode, "Pendulum Collision — Manual Rasterization");
    window.setFramerateLimit(60);

    sf::Image   img;
    sf::Texture tex;
    sf::Sprite  sprite;

    resetSimulation();

    sf::Clock clock;

    while (window.isOpen())
    {
        // ── Events ──
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::MouseButtonPressed)
                resetSimulation();
        }

        // ── Physics  (sub-stepped for stability) ──
        float dt = clock.restart().asSeconds();
        if (dt > 0.05f) dt = 0.05f;

        float subDt = dt / (float)SUB_STEPS;
        for (int i = 0; i < SUB_STEPS; ++i)
            physicsTick(subDt);

        // ── Rasterize ──
        clearBuffer(10, 10, 15);
        drawGrid();
        drawPivots();
        drawStrings();

        // Back-to-front by X  (simple depth cue)
        if (balls[0].x < balls[1].x)
        {
            drawBall(balls[0]);
            drawBall(balls[1]);
        }
        else
        {
            drawBall(balls[1]);
            drawBall(balls[0]);
        }

        // ── Blit pixel buffer to screen ──
        //   create(unsigned int width, unsigned int height, const Uint8* pixels)
        img.create(WIN_W, WIN_H, pixelBuf);
        tex.loadFromImage(img);
        sprite.setTexture(tex);

        window.clear();
        window.draw(sprite);

        // HUD on top
        drawHUD(window);

        window.display();
    }

    return 0;
}