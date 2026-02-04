// ============================================================================
//  TRAFFIC LIGHT SIMULATION
//  C++ / SFML — Pure manual graphics algorithms
// ============================================================================
//
//  WHAT THIS DOES:
//      - Traffic light cycles: Green → Yellow → Red → Green (repeats forever)
//      - One car drives from left to right
//      - GREEN light   → car drives at full speed
//      - YELLOW light  → car slows down
//      - RED light     → car stops completely
//      - When light turns green again, car accelerates back to full speed
//      - When car reaches the right edge, it wraps back to the left
//
//  GRAPHICS ALGORITHMS USED (all manual, no SFML shapes):
//      - Bresenham Line Algorithm   — draws the road, stop line, traffic pole
//      - Midpoint Circle Algorithm  — draws the three traffic lights (circles)
//      - Midpoint Ellipse Algorithm — draws the car wheels (ellipses)
//      - Rectangle fill (scanline)  — draws the car body
//
//  BUILD (Visual Studio):
//      Add SFML include/lib paths, link sfml-graphics/window/system, C++17
//
//  BUILD (g++):
//      g++ -O2 -std=c++17 TrafficSim.cpp -o TrafficSim
//          -I<sfml-include> -L<sfml-lib>
//          -lsfml-graphics -lsfml-window -lsfml-system
//
// ============================================================================

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <cmath>
#include <algorithm>

// ============================================================================
//  CONSTANTS
// ============================================================================
static const unsigned int WIN_W = 1000u;
static const unsigned int WIN_H = 600u;

// Traffic light timing (in seconds)
static const float GREEN_DURATION = 4.0f;   // green light lasts 4 seconds
static const float YELLOW_DURATION = 2.0f;   // yellow light lasts 2 seconds
static const float RED_DURATION = 3.0f;   // red light lasts 3 seconds

// Car physics
static const float CAR_SPEED_MAX = 200.0f;   // max speed: 200 pixels per second
static const float CAR_ACCEL = 150.0f;   // acceleration: 150 px/s²
static const float CAR_BRAKE = 250.0f;   // braking: 250 px/s²

// Traffic light position (top-right area)
static const int LIGHT_X = 800;   // x position of the traffic pole
static const int LIGHT_Y = 150;   // y position of top light
static const int LIGHT_RADIUS = 25;    // radius of each light circle
static const int LIGHT_SPACING = 70;   // vertical spacing between lights

// Stop line position (where the car should stop)
static const int STOP_LINE_X = 750;

// ============================================================================
//  PIXEL BUFFER — we draw everything manually into this array
// ============================================================================
static sf::Uint8 pixelBuf[WIN_W * WIN_H * 4];   // RGBA

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

static void setPixel(int x, int y, sf::Uint8 r, sf::Uint8 g, sf::Uint8 b, sf::Uint8 a = 255)
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
		// Alpha blending
		float fa = (float)a / 255.0f;
		float fb = ((float)pixelBuf[idx + 3] / 255.0f) * (1.0f - fa);
		pixelBuf[idx + 0] = (sf::Uint8)((float)r * fa + (float)pixelBuf[idx + 0] * fb);
		pixelBuf[idx + 1] = (sf::Uint8)((float)g * fa + (float)pixelBuf[idx + 1] * fb);
		pixelBuf[idx + 2] = (sf::Uint8)((float)b * fa + (float)pixelBuf[idx + 2] * fb);
		pixelBuf[idx + 3] = (sf::Uint8)((fa + fb) * 255.0f);
	}
}

// ============================================================================
//  BRESENHAM LINE ALGORITHM
//  Draws a straight line from (x0, y0) to (x1, y1)
// ============================================================================
static void bresenhamLine(int x0, int y0, int x1, int y1,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	int dx = std::abs(x1 - x0);
	int dy = -std::abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx + dy;

	for (;;)
	{
		setPixel(x0, y0, r, g, b);
		if (x0 == x1 && y0 == y1) break;

		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

// Thick line — draw the same line multiple times offset perpendicular
static void thickLine(int x0, int y0, int x1, int y1, int thickness,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	float dx = (float)(x1 - x0);
	float dy = (float)(y1 - y0);
	float len = (float)std::sqrt(dx * dx + dy * dy);

	if (len < 0.001f)
	{
		setPixel(x0, y0, r, g, b);
		return;
	}

	float px = -dy / len;   // perpendicular x
	float py = dx / len;   // perpendicular y

	float half = (float)(thickness - 1) * 0.5f;
	for (float i = -half; i <= half; i += 1.0f)
	{
		bresenhamLine((int)((float)x0 + px * i), (int)((float)y0 + py * i),
			(int)((float)x1 + px * i), (int)((float)y1 + py * i),
			r, g, b);
	}
}

// ============================================================================
//  MIDPOINT CIRCLE ALGORITHM
//  Draws a circle outline centered at (cx, cy) with given radius
// ============================================================================
static void midpointCircle(int cx, int cy, int radius,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	int x = radius;
	int y = 0;
	int p = 1 - radius;

	while (x >= y)
	{
		// Draw all 8 octants
		setPixel(cx + x, cy + y, r, g, b);
		setPixel(cx - x, cy + y, r, g, b);
		setPixel(cx + x, cy - y, r, g, b);
		setPixel(cx - x, cy - y, r, g, b);
		setPixel(cx + y, cy + x, r, g, b);
		setPixel(cx - y, cy + x, r, g, b);
		setPixel(cx + y, cy - x, r, g, b);
		setPixel(cx - y, cy - x, r, g, b);

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

// Scanline-fill a circle (solid circle)
static void fillCircle(int cx, int cy, int radius,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	if (radius < 1) return;

	for (int y = cy - radius; y <= cy + radius; ++y)
	{
		float dy = (float)(y - cy);
		float disc = (float)(radius * radius) - dy * dy;
		if (disc < 0.0f) continue;

		float sqD = (float)std::sqrt(disc);
		int   xLeft = (int)((float)cx - sqD);
		int   xRight = (int)((float)cx + sqD);

		for (int x = xLeft; x <= xRight; ++x)
			setPixel(x, y, r, g, b);
	}
}

// ============================================================================
//  MIDPOINT ELLIPSE ALGORITHM
//  Draws an ellipse centered at (cx, cy) with semi-axes (rx, ry)
// ============================================================================
static void midpointEllipse(int cx, int cy, int rx, int ry,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	// Region 1: where slope < -1
	int x = 0, y = ry;
	int rx2 = rx * rx;
	int ry2 = ry * ry;
	int twoRx2 = 2 * rx2;
	int twoRy2 = 2 * ry2;
	int p = (int)(ry2 - (rx2 * ry) + (0.25f * rx2));
	int px = 0;
	int py = twoRx2 * y;

	// Plot initial points (4-way symmetry)
	auto plot4 = [&](int dx, int dy) {
		setPixel(cx + dx, cy + dy, r, g, b);
		setPixel(cx - dx, cy + dy, r, g, b);
		setPixel(cx + dx, cy - dy, r, g, b);
		setPixel(cx - dx, cy - dy, r, g, b);
		};

	plot4(x, y);

	// Region 1
	while (px < py)
	{
		++x;
		px += twoRy2;
		if (p < 0)
			p += ry2 + px;
		else
		{
			--y;
			py -= twoRx2;
			p += ry2 + px - py;
		}
		plot4(x, y);
	}

	// Region 2: where slope >= -1
	p = (int)(ry2 * (x + 0.5f) * (x + 0.5f) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
	while (y > 0)
	{
		--y;
		py -= twoRx2;
		if (p > 0)
			p += rx2 - py;
		else
		{
			++x;
			px += twoRy2;
			p += rx2 - py + px;
		}
		plot4(x, y);
	}
}

// Scanline-fill an ellipse (solid)
static void fillEllipse(int cx, int cy, int rx, int ry,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	if (rx < 1 || ry < 1) return;

	for (int y = cy - ry; y <= cy + ry; ++y)
	{
		float dy = (float)(y - cy);
		float disc = (float)(ry * ry) - dy * dy;
		if (disc < 0.0f) continue;

		float sqD = (float)std::sqrt(disc) * ((float)rx / (float)ry);
		int   xLeft = (int)((float)cx - sqD);
		int   xRight = (int)((float)cx + sqD);

		for (int x = xLeft; x <= xRight; ++x)
			setPixel(x, y, r, g, b);
	}
}

// ============================================================================
//  FILLED RECTANGLE (scanline)
// ============================================================================
static void fillRect(int x, int y, int w, int h,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	for (int dy = 0; dy < h; ++dy)
		for (int dx = 0; dx < w; ++dx)
			setPixel(x + dx, y + dy, r, g, b);
}

// ============================================================================
//  TRAFFIC LIGHT STATE
// ============================================================================
enum LightState
{
	LIGHT_GREEN,
	LIGHT_YELLOW,
	LIGHT_RED
};

struct TrafficLight
{
	LightState state;
	float      timer;   // time left in current state

	TrafficLight() : state(LIGHT_GREEN), timer(GREEN_DURATION) {}

	// Update the light every frame
	void update(float dt)
	{
		timer -= dt;
		if (timer <= 0.0f)
		{
			// Cycle to next state
			if (state == LIGHT_GREEN)
			{
				state = LIGHT_YELLOW;
				timer = YELLOW_DURATION;
			}
			else if (state == LIGHT_YELLOW)
			{
				state = LIGHT_RED;
				timer = RED_DURATION;
			}
			else   // RED
			{
				state = LIGHT_GREEN;
				timer = GREEN_DURATION;
			}
		}
	}

	// Draw the traffic light (3 circles: top=red, middle=yellow, bottom=green)
	void draw() const
	{
		// Draw the pole (vertical line)
		thickLine(LIGHT_X, 100, LIGHT_X, LIGHT_Y + LIGHT_SPACING * 2 + 50, 8, 50, 50, 50);

		// Draw the housing (black rectangle behind the lights)
		fillRect(LIGHT_X - 40, LIGHT_Y - 35, 80, LIGHT_SPACING * 2 + 70, 30, 30, 30);

		// Top light (RED)
		int redY = LIGHT_Y;
		if (state == LIGHT_RED)
			fillCircle(LIGHT_X, redY, LIGHT_RADIUS, 255, 0, 0);   // bright red
		else
			fillCircle(LIGHT_X, redY, LIGHT_RADIUS, 60, 0, 0);    // dark red (off)
		midpointCircle(LIGHT_X, redY, LIGHT_RADIUS, 100, 100, 100);   // grey outline

		// Middle light (YELLOW)
		int yellowY = LIGHT_Y + LIGHT_SPACING;
		if (state == LIGHT_YELLOW)
			fillCircle(LIGHT_X, yellowY, LIGHT_RADIUS, 255, 220, 0);   // bright yellow
		else
			fillCircle(LIGHT_X, yellowY, LIGHT_RADIUS, 60, 50, 0);     // dark yellow (off)
		midpointCircle(LIGHT_X, yellowY, LIGHT_RADIUS, 100, 100, 100);

		// Bottom light (GREEN)
		int greenY = LIGHT_Y + LIGHT_SPACING * 2;
		if (state == LIGHT_GREEN)
			fillCircle(LIGHT_X, greenY, LIGHT_RADIUS, 0, 255, 0);   // bright green
		else
			fillCircle(LIGHT_X, greenY, LIGHT_RADIUS, 0, 60, 0);    // dark green (off)
		midpointCircle(LIGHT_X, greenY, LIGHT_RADIUS, 100, 100, 100);
	}
};

// ============================================================================
//  CAR
// ============================================================================
struct Car
{
	float x;         // x position (center of car)
	float y;         // y position (center of car)
	float velocity;  // current speed in pixels per second

	int width;       // car body width
	int height;      // car body height

	Car() : x(100.0f), y(400.0f), velocity(CAR_SPEED_MAX),
		width(100), height(50) {
	}

	// Update car position based on traffic light state
	void update(float dt, const TrafficLight& light)
	{
		// Decide what to do based on traffic light
		if (light.state == LIGHT_GREEN)
		{
			// GREEN — accelerate to max speed
			if (velocity < CAR_SPEED_MAX)
			{
				velocity += CAR_ACCEL * dt;
				if (velocity > CAR_SPEED_MAX)
					velocity = CAR_SPEED_MAX;
			}
		}
		else if (light.state == LIGHT_YELLOW)
		{
			// YELLOW — slow down
			// Only brake if we haven't already stopped
			if (velocity > 0.0f && x < STOP_LINE_X - width / 2)
			{
				velocity -= CAR_BRAKE * dt;
				if (velocity < 0.0f)
					velocity = 0.0f;
			}
		}
		else   // RED
		{
			// RED — stop if we haven't passed the stop line yet
			if (x < STOP_LINE_X - width / 2)
			{
				if (velocity > 0.0f)
				{
					velocity -= CAR_BRAKE * dt;
					if (velocity < 0.0f)
						velocity = 0.0f;
				}
			}
		}

		// Move forward
		x += velocity * dt;

		// Wrap around if we go off the right edge
		if (x > (float)WIN_W + 100.0f)
			x = -100.0f;
	}

	// Draw the car — body (rectangle) + wheels (ellipses) + windows
	void draw() const
	{
		int carX = (int)x;
		int carY = (int)y;

		// Car body (blue rectangle)
		fillRect(carX - width / 2, carY - height / 2, width, height, 50, 100, 200);

		// Outline of body
		thickLine(carX - width / 2, carY - height / 2,
			carX + width / 2, carY - height / 2, 2, 30, 60, 120);   // top
		thickLine(carX - width / 2, carY + height / 2,
			carX + width / 2, carY + height / 2, 2, 30, 60, 120);   // bottom
		thickLine(carX - width / 2, carY - height / 2,
			carX - width / 2, carY + height / 2, 2, 30, 60, 120);   // left
		thickLine(carX + width / 2, carY - height / 2,
			carX + width / 2, carY + height / 2, 2, 30, 60, 120);   // right

		// Windows (lighter blue rectangles on top half)
		fillRect(carX - width / 2 + 10, carY - height / 2 + 5, 30, 15, 150, 200, 255);   // left window
		fillRect(carX + width / 2 - 40, carY - height / 2 + 5, 30, 15, 150, 200, 255);   // right window

		// Wheels (black filled ellipses)
		int wheelRx = 12;
		int wheelRy = 8;
		int wheelY = carY + height / 2 + 5;
		int wheel1X = carX - width / 2 + 20;
		int wheel2X = carX + width / 2 - 20;

		fillEllipse(wheel1X, wheelY, wheelRx, wheelRy, 20, 20, 20);   // left wheel
		fillEllipse(wheel2X, wheelY, wheelRx, wheelRy, 20, 20, 20);   // right wheel

		// Wheel outlines
		midpointEllipse(wheel1X, wheelY, wheelRx, wheelRy, 80, 80, 80);
		midpointEllipse(wheel2X, wheelY, wheelRx, wheelRy, 80, 80, 80);
	}
};

// ============================================================================
//  DRAW SCENE
// ============================================================================
static void drawRoad()
{
	// Road (grey horizontal band)
	fillRect(0, 380, WIN_W, 100, 60, 60, 65);

	// Road lane markings (dashed white lines)
	int laneY = 430;
	for (int x = 0; x < (int)WIN_W; x += 50)
		thickLine(x, laneY, x + 30, laneY, 3, 255, 255, 255);

	// Stop line (thick white vertical line before the traffic light)
	thickLine(STOP_LINE_X, 380, STOP_LINE_X, 480, 5, 255, 255, 255);
}

static void drawHUD(sf::RenderWindow& window, const TrafficLight& light, const Car& car)
{
	// Light state text
	sf::Text t1;
	char buf[128];
	const char* stateStr = (light.state == LIGHT_GREEN) ? "GREEN" :
		(light.state == LIGHT_YELLOW) ? "YELLOW" : "RED";
	std::snprintf(buf, sizeof(buf), "Light: %s  (%.1fs)", stateStr, light.timer);
	t1.setString(buf);
	t1.setCharacterSize(18);
	t1.setFillColor(sf::Color(255, 255, 255));
	t1.setPosition(20, 20);
	window.draw(t1);

	// Car velocity text
	sf::Text t2;
	std::snprintf(buf, sizeof(buf), "Car Speed: %.0f px/s  (Max: %.0f)", car.velocity, CAR_SPEED_MAX);
	t2.setString(buf);
	t2.setCharacterSize(16);
	t2.setFillColor(sf::Color(255, 255, 255));
	t2.setPosition(20, 50);
	window.draw(t2);

	// Instructions
	sf::Text t3;
	t3.setString("Traffic Simulation — Car obeys traffic light | Close window to quit");
	t3.setCharacterSize(14);
	t3.setFillColor(sf::Color(180, 180, 180));
	t3.setPosition(20, WIN_H - 30);
	window.draw(t3);
}

// ============================================================================
//  MAIN
// ============================================================================
int main()
{
	sf::VideoMode    mode(WIN_W, WIN_H);
	sf::RenderWindow window(mode, "Traffic Light Simulation — Manual Graphics Algorithms");
	window.setFramerateLimit(60);

	sf::Image   img;
	sf::Texture tex;
	sf::Sprite  sprite;

	TrafficLight light;
	Car          car;

	sf::Clock clock;

	while (window.isOpen())
	{
		// Events
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}

		// Delta time
		float dt = clock.restart().asSeconds();
		if (dt > 0.05f) dt = 0.05f;   // cap to avoid huge jumps

		// Update
		light.update(dt);
		car.update(dt, light);

		// Draw
		clearBuffer(30, 120, 50);   // greenish background (sky/grass)
		drawRoad();
		light.draw();
		car.draw();

		// Blit pixel buffer to screen
		img.create(WIN_W, WIN_H, pixelBuf);
		tex.loadFromImage(img);
		sprite.setTexture(tex);

		window.clear();
		window.draw(sprite);

		// HUD on top
		drawHUD(window, light, car);

		window.display();
	}

	return 0;
}