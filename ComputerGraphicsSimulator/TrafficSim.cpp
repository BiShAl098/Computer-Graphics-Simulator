// ============================================================================
//  TRAFFIC LIGHT SIMULATION
//  C++ / SFML — Pure manual graphics algorithms
// ============================================================================
//
//  WHAT THIS DOES:
//      - Traffic light controlled by YOU (click to change it)
//      - One car drives from left to right
//      - GREEN light   → car drives at full speed (200 px/s)
//      - YELLOW light  → car slows down to 40% speed (80 px/s) but keeps moving
//      - RED light     → car stops completely at the stop line
//      - Click anywhere to cycle: GREEN → YELLOW → RED → GREEN
//      - Car obeys whatever light you set
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
//  ROTATED LINE (for wheel spokes)
//  Draws a line from center (cx, cy) at given angle and length
// ============================================================================
static void drawSpoke(int cx, int cy, float angle, int length,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	int x1 = cx + (int)(std::cos(angle) * length);
	int y1 = cy + (int)(std::sin(angle) * length);
	thickLine(cx, cy, x1, y1, 2, r, g, b);
}

// ============================================================================
//  CIRCULAR ARC (for speedometer gauge)
//  Draws an arc from startAngle to endAngle
// ============================================================================
static void drawArc(int cx, int cy, int radius, float startAngle, float endAngle,
	sf::Uint8 r, sf::Uint8 g, sf::Uint8 b)
{
	float step = 0.05f;   // angle step size (smaller = smoother)
	for (float angle = startAngle; angle < endAngle; angle += step)
	{
		int x1 = cx + (int)(std::cos(angle) * radius);
		int y1 = cy + (int)(std::sin(angle) * radius);
		int x2 = cx + (int)(std::cos(angle + step) * radius);
		int y2 = cy + (int)(std::sin(angle + step) * radius);
		thickLine(x1, y1, x2, y2, 3, r, g, b);
	}
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

	TrafficLight() : state(LIGHT_GREEN) {}   // start on GREEN

	// Cycle to the next light state (called on click)
	void cycle()
	{
		if (state == LIGHT_GREEN)
			state = LIGHT_YELLOW;
		else if (state == LIGHT_YELLOW)
			state = LIGHT_RED;
		else   // RED
			state = LIGHT_GREEN;
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

	float wheelRotation;   // wheel rotation angle in radians (for animation)

	Car() : x(100.0f), y(400.0f), velocity(CAR_SPEED_MAX),
		width(150), height(70), wheelRotation(0.0f) {
	}   // BIGGER: 150x70 (was 100x50)

// Update car position based on traffic light state
	void update(float dt, const TrafficLight& light, const Car* otherCar = nullptr)
	{
		// Check if we're AT or past the stop line (front of car touched/passed it)
		// Front of car is at: x + width/2
		// If front >= stop line, we've touched it
		bool atOrPastStopLine = (x + width / 2 >= STOP_LINE_X);

		// Check if there's a car in front of us (within safe distance)
		bool carAhead = false;
		float safeDistance = 80.0f;   // maintain 80px gap between cars

		if (otherCar != nullptr)
		{
			// Only care if the other car is ahead of us
			if (otherCar->x > x)
			{
				float gap = otherCar->x - x;   // distance between centers
				float minGap = (width + otherCar->width) / 2.0f + safeDistance;

				if (gap < minGap)
					carAhead = true;
			}
		}

		// Decide what to do based on traffic light AND position
		if (carAhead)
		{
			// CAR AHEAD — match their speed or brake to avoid collision
			if (otherCar->velocity < velocity)
			{
				// They're slower — brake to match their speed
				velocity -= CAR_BRAKE * dt;
				if (velocity < otherCar->velocity)
					velocity = otherCar->velocity;
			}
		}
		else if (atOrPastStopLine)
		{
			// At or past the stop line — ALWAYS accelerate to full speed (ignore the light)
			if (velocity < CAR_SPEED_MAX)
			{
				velocity += CAR_ACCEL * dt;
				if (velocity > CAR_SPEED_MAX)
					velocity = CAR_SPEED_MAX;
			}
		}
		else if (light.state == LIGHT_GREEN)
		{
			// GREEN and before stop line — accelerate to max speed
			if (velocity < CAR_SPEED_MAX)
			{
				velocity += CAR_ACCEL * dt;
				if (velocity > CAR_SPEED_MAX)
					velocity = CAR_SPEED_MAX;
			}
		}
		else if (light.state == LIGHT_YELLOW)
		{
			// YELLOW and before stop line — slow down but keep moving
			if (velocity > CAR_SPEED_MAX * 0.4f)   // slow down to 40% of max speed
			{
				velocity -= CAR_BRAKE * dt;
				if (velocity < CAR_SPEED_MAX * 0.4f)
					velocity = CAR_SPEED_MAX * 0.4f;
			}
		}
		else   // RED and before stop line
		{
			// RED — stop completely
			if (velocity > 0.0f)
			{
				velocity -= CAR_BRAKE * dt;
				if (velocity < 0.0f)
					velocity = 0.0f;
			}
		}

		// Move forward
		x += velocity * dt;

		// Rotate wheels based on velocity
		// Wheel circumference ≈ 2π * wheelRadius
		// wheelRadius = 12 (the horizontal radius)
		// rotationSpeed = velocity / circumference
		float wheelCircumference = 2.0f * 3.14159f * 12.0f;   // 2πr
		wheelRotation += (velocity / wheelCircumference) * 2.0f * 3.14159f * dt;

		// Keep angle in [0, 2π) range to avoid overflow
		while (wheelRotation > 6.28318f)   // 2π
			wheelRotation -= 6.28318f;

		// Wrap around if we go off the right edge
		if (x > (float)WIN_W + 100.0f)
			x = -100.0f;
	}

	// Draw the car — body (rectangle) + wheels (ellipses with spokes) + windows + BRAKE LIGHTS
	void draw() const
	{
		int carX = (int)x;
		int carY = (int)y;

		// Car body (blue rectangle)
		fillRect(carX - width / 2, carY - height / 2, width, height, 50, 100, 200);

		// Outline of body
		thickLine(carX - width / 2, carY - height / 2,
			carX + width / 2, carY - height / 2, 3, 30, 60, 120);   // top
		thickLine(carX - width / 2, carY + height / 2,
			carX + width / 2, carY + height / 2, 3, 30, 60, 120);   // bottom
		thickLine(carX - width / 2, carY - height / 2,
			carX - width / 2, carY + height / 2, 3, 30, 60, 120);   // left
		thickLine(carX + width / 2, carY - height / 2,
			carX + width / 2, carY + height / 2, 3, 30, 60, 120);   // right

		// Windows (lighter blue rectangles on top half)
		fillRect(carX - width / 2 + 15, carY - height / 2 + 8, 40, 20, 150, 200, 255);   // left window
		fillRect(carX + width / 2 - 55, carY - height / 2 + 8, 40, 20, 150, 200, 255);   // right window

		// BRAKE LIGHTS (rear of car — glow RED when slowing down)
		// If velocity < max speed, we're braking
		bool braking = (velocity < CAR_SPEED_MAX - 10.0f);
		int rearX = carX - width / 2;

		if (braking)
		{
			// Bright red brake lights
			fillRect(rearX - 8, carY - height / 2 + 10, 8, 12, 255, 0, 0);     // left brake light
			fillRect(rearX - 8, carY + height / 2 - 22, 8, 12, 255, 0, 0);     // right brake light
		}
		else
		{
			// Dark red (off)
			fillRect(rearX - 8, carY - height / 2 + 10, 8, 12, 80, 0, 0);
			fillRect(rearX - 8, carY + height / 2 - 22, 8, 12, 80, 0, 0);
		}

		// Wheels (BIGGER ellipses with rotating spokes)
		int wheelRx = 18;   // horizontal radius (was 12)
		int wheelRy = 12;   // vertical radius (was 8)
		int wheelY = carY + height / 2 + 8;
		int wheel1X = carX - width / 2 + 30;
		int wheel2X = carX + width / 2 - 30;

		// Left wheel
		fillEllipse(wheel1X, wheelY, wheelRx, wheelRy, 20, 20, 20);   // black tire
		midpointEllipse(wheel1X, wheelY, wheelRx, wheelRy, 80, 80, 80);   // grey outline

		// Left wheel spokes (4 spokes rotating)
		for (int i = 0; i < 4; ++i)
		{
			float spokeAngle = wheelRotation + (float)i * (3.14159f / 2.0f);   // 90° apart
			drawSpoke(wheel1X, wheelY, spokeAngle, wheelRx - 4, 120, 120, 120);
		}

		// Center hub (small grey circle)
		fillCircle(wheel1X, wheelY, 4, 80, 80, 80);

		// Right wheel (same as left)
		fillEllipse(wheel2X, wheelY, wheelRx, wheelRy, 20, 20, 20);
		midpointEllipse(wheel2X, wheelY, wheelRx, wheelRy, 80, 80, 80);

		// Right wheel spokes
		for (int i = 0; i < 4; ++i)
		{
			float spokeAngle = wheelRotation + (float)i * (3.14159f / 2.0f);
			drawSpoke(wheel2X, wheelY, spokeAngle, wheelRx - 4, 120, 120, 120);
		}

		// Center hub
		fillCircle(wheel2X, wheelY, 4, 80, 80, 80);
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
	std::snprintf(buf, sizeof(buf), "Light: %s", stateStr);
	t1.setString(buf);
	t1.setCharacterSize(20);
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
	t3.setString("CLICK ANYWHERE to change the traffic light  |  Car obeys the light");
	t3.setCharacterSize(15);
	t3.setFillColor(sf::Color(220, 220, 220));
	t3.setPosition(20, WIN_H - 35);
	window.draw(t3);
}

// ============================================================================
//  SPEEDOMETER GAUGE (drawn in pixel buffer)
//  Visual arc showing current speed
// ============================================================================
static void drawSpeedometer(const Car& car)
{
	int gaugeX = 120;
	int gaugeY = 130;
	int gaugeRadius = 50;

	// Background arc (dark grey — shows the full range)
	float startAngle = 3.14159f * 0.75f;   // 135° (bottom-left)
	float endAngle = 3.14159f * 2.25f;   // 405° (bottom-right) = 270° total range
	drawArc(gaugeX, gaugeY, gaugeRadius, startAngle, endAngle, 60, 60, 60);

	// Speed needle arc (colored based on speed)
	float speedRatio = car.velocity / CAR_SPEED_MAX;   // 0.0 to 1.0
	float needleAngle = startAngle + speedRatio * (endAngle - startAngle);

	// Color: green when slow, yellow when medium, red when fast
	sf::Uint8 needleR = (sf::Uint8)(255 * speedRatio);
	sf::Uint8 needleG = (sf::Uint8)(255 * (1.0f - speedRatio));
	sf::Uint8 needleB = 0;

	drawArc(gaugeX, gaugeY, gaugeRadius, startAngle, needleAngle, needleR, needleG, needleB);

	// Center dot
	fillCircle(gaugeX, gaugeY, 5, 100, 100, 100);
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
	Car          car1;    // first car
	Car          car2;    // second car (follows behind)

	// Position car2 behind car1
	car2.x = car1.x - 250.0f;   // 250px behind

	sf::Clock clock;

	while (window.isOpen())
	{
		// Events
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();

			// LEFT CLICK anywhere → cycle the traffic light
			if (event.type == sf::Event::MouseButtonPressed)
			{
				if (event.mouseButton.button == sf::Mouse::Left)
					light.cycle();
			}
		}

		// Delta time
		float dt = clock.restart().asSeconds();
		if (dt > 0.05f) dt = 0.05f;   // cap to avoid huge jumps

		// Update cars (each car checks the other car for collision avoidance)
		car1.update(dt, light, &car2);   // car1 checks car2
		car2.update(dt, light, &car1);   // car2 checks car1

		// Draw
		clearBuffer(30, 120, 50);   // greenish background (sky/grass)
		drawRoad();
		drawSpeedometer(car1);   // speedometer gauge (top-left) — shows car1's speed
		light.draw();

		// Draw cars back-to-front (whoever is further left draws first)
		if (car1.x < car2.x)
		{
			car1.draw();
			car2.draw();
		}
		else
		{
			car2.draw();
			car1.draw();
		}

		// Blit pixel buffer to screen
		img.create(WIN_W, WIN_H, pixelBuf);
		tex.loadFromImage(img);
		sprite.setTexture(tex);

		window.clear();
		window.draw(sprite);

		// HUD on top
		drawHUD(window, light, car1);

		window.display();
	}

	return 0;
}