/*
 * ============================================================
 *  PENDULUM COLLISION SIMULATION  +  MANUAL SCALING
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
 *  Scaling (manual math, no library calls):
 *      - Each ball owns: baseRadius, scaleFactor, scaleTarget
 *      - On click: scaleTarget = clamp(random, SCALE_MIN, SCALE_MAX)
 *                  independently for each ball
 *      - Every frame: scaleFactor lerps toward scaleTarget
 *      - drawRadius = (int)(baseRadius * scaleFactor)   <-- applied
 *                     to scanline fill, glow, outline, AND collision
 *
 *  Physics:
 *      - Pendulum torque:  a = -(g / L) * sin(theta)
 *      - Elastic collision uses scaled radii for minDist
 *      - Sub-stepping (8 steps/frame) for stability
 *
 *  Build (Visual Studio):
 *      Add SFML include/lib paths in project properties.
 *      Link: sfml-graphics.lib  sfml-window.lib  sfml-system.lib
 *      Compile as C++17.
 *
 *  Controls:
 *      Left click   — randomise both ball scales (clamped)
 *      Close window — quit
 * ============================================================
 */

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>

 // ════════════════════════════════════════════════════════════════════════════
 //  CONSTANTS
 // ════════════════════════════════════════════════════════════════════════════
static const unsigned int WIN_W = 900u;
static const unsigned int WIN_H = 700u;
static const float        GRAVITY = 980.0f;
static const float        DAMPING = 0.9996f;
static const int          SUB_STEPS = 8;
static const int          BALL_BASE_RADIUS = 28;   // base radius before scaling

// ── Scaling clamp range ──
static const float SCALE_MIN = 0.4f;   // smallest allowed scale
static const float SCALE_MAX = 2.6f;   // largest allowed scale
static const float SCALE_LERP = 4.0f;   // how fast scaleFactor chases scaleTarget (per second)

// ════════════════════════════════════════════════════════════════════════════
//  PIXEL BUFFER
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
//  MIDPOINT CIRCLE  (outline)
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
            p += 2 * y + 1;
        else
        {
            --x;
            p += 2 * (y - x) + 1;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  SCANLINE FILL  —  Lambert + Phong + Fresnel
// ════════════════════════════════════════════════════════════════════════════
static const float LIGHT_LX = -0.5f / 0.8602f;
static const float LIGHT_LY = -0.7f / 0.8602f;

static void scanlineFillCircle(int cx, int cy, int radius,
    sf::Uint8 baseR, sf::Uint8 baseG, sf::Uint8 baseB,
    sf::Uint8 glowR, sf::Uint8 glowG, sf::Uint8 glowB)
{
    if (radius < 1) return;
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

            float nx = dx * invR;
            float ny = dy * invR;
            float nz2 = 1.0f - nx * nx - ny * ny;
            float nz = (nz2 > 0.0f) ? (float)std::sqrt((double)nz2) : 0.0f;

            float diff = nx * LIGHT_LX + ny * LIGHT_LY + nz * 0.7f;
            if (diff < 0.0f) diff = 0.0f;

            float spec = nz * 0.9f + diff * 0.3f;
            if (spec < 0.0f) spec = 0.0f;
            spec = spec * spec;
            spec = spec * spec;
            spec = spec * spec;   // ^8

            float shade = 0.12f + diff * 0.70f + spec * 0.50f;
            if (shade > 1.4f) shade = 1.4f;

            float fR = (float)baseR * shade;
            float fG = (float)baseG * shade;
            float fB = (float)baseB * shade;

            float rim = dist * dist;
            fR += (float)glowR * rim * 0.30f;
            fG += (float)glowG * rim * 0.30f;
            fB += (float)glowB * rim * 0.30f;

            setPixel(x, y,
                (sf::Uint8)std::min(fR, 255.0f),
                (sf::Uint8)std::min(fG, 255.0f),
                (sf::Uint8)std::min(fB, 255.0f), 255);
        }
    }
}

static void scanlineGlowRing(int cx, int cy, int radius,
    sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
    if (radius < 1) return;
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
    for (int x = 0; x < (int)WIN_W; x += STEP)
        for (int y = 0; y < (int)WIN_H; ++y)
            setPixel(x, y, 20, 20, 30, 35);

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
    float     angle;
    float     angularVel;

    int       baseRadius;     // original radius (never changes)
    float     scaleFactor;    // current scale (animated)
    float     scaleTarget;    // target scale (set on click)

    float     x, y;

    sf::Uint8 baseR, baseG, baseB;
    sf::Uint8 glowR, glowG, glowB;

    // ── Manual scaling math ──────────────────────────────────────────
    //   scaledRadius = baseRadius * scaleFactor
    //   This is the ONLY place scaling is computed.
    //   Everything that draws or collides reads scaledRadius().
    int scaledRadius() const
    {
        // multiply base by current factor, truncate to int
        int r = (int)((float)baseRadius * scaleFactor);
        return (r < 1) ? 1 : r;   // never let it go to 0
    }

    void updatePosition()
    {
        x = pivotX + length * (float)std::sin((double)angle);
        y = pivotY + length * (float)std::cos((double)angle);
    }

    // Lerp scaleFactor toward scaleTarget each frame
    void updateScale(float dt)
    {
        float diff = scaleTarget - scaleFactor;
        // exponential ease: move a fraction of the remaining gap
        float move = diff * SCALE_LERP * dt;
        // if move overshoots, just snap
        if ((diff > 0.0f && move > diff) || (diff < 0.0f && move < diff))
            scaleFactor = scaleTarget;
        else
            scaleFactor += move;
    }
};

// ════════════════════════════════════════════════════════════════════════════
//  GLOBALS
// ════════════════════════════════════════════════════════════════════════════
static Ball  balls[2];
static float globalPivotY;

// ── Simple pseudo-random float in [0, 1) ────────────────────────────────
static float randFloat()
{
    return (float)std::rand() / ((float)RAND_MAX + 1.0f);
}

// ── Generate a random scale clamped to [SCALE_MIN, SCALE_MAX] ──────────
static float randomClampedScale()
{
    // random value in [0,1) mapped to [SCALE_MIN, SCALE_MAX]
    float r = SCALE_MIN + randFloat() * (SCALE_MAX - SCALE_MIN);
    // clamp (belt & suspenders)
    if (r < SCALE_MIN) r = SCALE_MIN;
    if (r > SCALE_MAX) r = SCALE_MAX;
    return r;
}

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
    balls[0].baseRadius = BALL_BASE_RADIUS;
    balls[0].scaleFactor = 1.0f;
    balls[0].scaleTarget = 1.0f;
    balls[0].baseR = 220; balls[0].baseG = 80; balls[0].baseB = 50;
    balls[0].glowR = 255; balls[0].glowG = 120; balls[0].glowB = 60;
    balls[0].updatePosition();

    // ── Ball B  (right, ice) ──
    balls[1].pivotX = cx + 60.0f;
    balls[1].pivotY = globalPivotY;
    balls[1].length = 240.0f;
    balls[1].angle = 0.60f;
    balls[1].angularVel = 0.0f;
    balls[1].baseRadius = BALL_BASE_RADIUS;
    balls[1].scaleFactor = 1.0f;
    balls[1].scaleTarget = 1.0f;
    balls[1].baseR = 50; balls[1].baseG = 130; balls[1].baseB = 220;
    balls[1].glowR = 80; balls[1].glowG = 180; balls[1].glowB = 255;
    balls[1].updatePosition();
}

// Called on every left-click: both balls get NEW independent random targets
static void randomiseScales()
{
    balls[0].scaleTarget = randomClampedScale();
    balls[1].scaleTarget = randomClampedScale();
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

    // ── 2) Elastic collision  (uses scaledRadius for both balls) ──
    Ball& A = balls[0];
    Ball& B = balls[1];

    float dx = B.x - A.x;
    float dy = B.y - A.y;
    float dist = (float)std::sqrt((double)(dx * dx + dy * dy));

    // Collision distance = sum of SCALED radii
    float minD = (float)(A.scaledRadius() + B.scaledRadius());

    if (dist < minD && dist > 0.001f)
    {
        float nx = dx / dist;
        float ny = dy / dist;

        float vAx = A.angularVel * A.length * (float)std::cos((double)A.angle);
        float vAy = -A.angularVel * A.length * (float)std::sin((double)A.angle);
        float vBx = B.angularVel * B.length * (float)std::cos((double)B.angle);
        float vBy = -B.angularVel * B.length * (float)std::sin((double)B.angle);

        float relVn = (vAx - vBx) * nx + (vAy - vBy) * ny;

        if (relVn > 0.0f)
        {
            float j = relVn;

            float tAx = (float)std::cos((double)A.angle);
            float tAy = -(float)std::sin((double)A.angle);
            float tBx = (float)std::cos((double)B.angle);
            float tBy = -(float)std::sin((double)B.angle);

            float dOmA = -(j * (nx * tAx + ny * tAy)) / A.length;
            float dOmB = (j * (nx * tBx + ny * tBy)) / B.length;

            A.angularVel += dOmA;
            B.angularVel += dOmB;

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
//  DRAW BALL   — everything uses scaledRadius()
// ════════════════════════════════════════════════════════════════════════════
static void drawBall(const Ball& b)
{
    int cx = (int)b.x;
    int cy = (int)b.y;
    int sr = b.scaledRadius();   // <── scaled radius used for ALL drawing

    // Glow ring is 6 px bigger than scaled radius
    scanlineGlowRing(cx, cy, sr + 6,
        b.glowR, b.glowG, b.glowB);

    // Main body
    scanlineFillCircle(cx, cy, sr,
        b.baseR, b.baseG, b.baseB,
        b.glowR, b.glowG, b.glowB);

    // Outline
    midpointCircle(cx, cy, sr,
        b.glowR, b.glowG, b.glowB, 180);

    // Highlight ring — scales with the ball too
    //   offset toward light, radius = 55% of scaled radius
    int highlightR = (int)((float)sr * 0.55f);
    midpointCircle(cx - 2, cy - 2, highlightR,
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
//  HUD
// ════════════════════════════════════════════════════════════════════════════
static void drawHUD(sf::RenderWindow& window)
{
    const float RAD2DEG = 180.0f / 3.14159265358979f;

    // ── Ball A  (angle + scale) ──
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
            "A   theta: %6.1f deg   omega: %6.3f   scale: %.2f x  [target: %.2f x]",
            balls[0].angle * RAD2DEG,
            balls[0].angularVel,
            balls[0].scaleFactor,
            balls[0].scaleTarget);
        sf::Text t;
        t.setString(buf);
        t.setCharacterSize(12);
        t.setFillColor(sf::Color(192, 82, 44));
        t.setPosition(14.0f, (float)(WIN_H - 42));
        window.draw(t);
    }

    // ── Ball B  (angle + scale) ──
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
            "B   theta: %6.1f deg   omega: %6.3f   scale: %.2f x  [target: %.2f x]",
            balls[1].angle * RAD2DEG,
            balls[1].angularVel,
            balls[1].scaleFactor,
            balls[1].scaleTarget);
        sf::Text t;
        t.setString(buf);
        t.setCharacterSize(12);
        t.setFillColor(sf::Color(44, 122, 192));
        t.setPosition(14.0f, (float)(WIN_H - 22));
        window.draw(t);
    }

    // ── Collision flash ──
    {
        float dx = balls[1].x - balls[0].x;
        float dy = balls[1].y - balls[0].y;
        float dist = (float)std::sqrt((double)(dx * dx + dy * dy));
        float minD = (float)(balls[0].scaledRadius() + balls[1].scaledRadius());

        if (dist < minD + 5.0f)
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
        t.setString("PENDULUM COLLISION + SCALING SIMULATION");
        t.setCharacterSize(11);
        t.setFillColor(sf::Color(70, 70, 90));
        t.setPosition((float)WIN_W * 0.5f - 175.0f, 10.0f);
        window.draw(t);
    }

    // ── Sub-title ──
    {
        sf::Text t;
        t.setString("Bresenham  |  Midpoint Circle  |  Scanline Fill  |  Manual Scaling  |  Physics");
        t.setCharacterSize(10);
        t.setFillColor(sf::Color(55, 55, 70));
        t.setPosition((float)WIN_W * 0.5f - 230.0f, 24.0f);
        window.draw(t);
    }

    // ── Scale range info ──
    {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "Click to randomise scale  [%.1f x  —  %.1f x]",
            SCALE_MIN, SCALE_MAX);
        sf::Text t;
        t.setString(buf);
        t.setCharacterSize(10);
        t.setFillColor(sf::Color(40, 40, 55));
        t.setPosition(14.0f, (float)(WIN_H - 60));
        window.draw(t);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════════════════════
int main()
{
    std::srand((unsigned int)std::time(nullptr));   // seed RNG once

    sf::VideoMode    mode(WIN_W, WIN_H);
    sf::RenderWindow window(mode, "Pendulum Collision + Scaling");
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
            {
                // Left click  → randomise scales (both balls, independently)
                // Right click → full reset
                if (event.mouseButton.button == sf::Mouse::Left)
                    randomiseScales();
                else if (event.mouseButton.button == sf::Mouse::Right)
                    resetSimulation();
            }
        }

        // ── Delta time ──
        float dt = clock.restart().asSeconds();
        if (dt > 0.05f) dt = 0.05f;

        // ── Update scale lerp (every frame, outside sub-step) ──
        for (int i = 0; i < 2; ++i)
            balls[i].updateScale(dt);

        // ── Physics  (sub-stepped) ──
        float subDt = dt / (float)SUB_STEPS;
        for (int i = 0; i < SUB_STEPS; ++i)
            physicsTick(subDt);

        // ── Rasterize ──
        clearBuffer(10, 10, 15);
        drawGrid();
        drawPivots();
        drawStrings();

        // Back-to-front
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

        // ── Blit ──
        img.create(WIN_W, WIN_H, pixelBuf);
        tex.loadFromImage(img);
        sprite.setTexture(tex);

        window.clear();
        window.draw(sprite);

        drawHUD(window);
        window.display();
    }

    return 0;
}