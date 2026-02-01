#include <SFML/Graphics.hpp>
#include <vector>

// ============================================================================
// SIMPLE CUSTOM MATH (NO LIBRARIES)
// ============================================================================
namespace Math {
	const float PI = 3.14159265f;

	float abs(float x) { return x < 0 ? -x : x; }

	float sqrt(float x) {
		if (x <= 0) return 0;
		float result = x;
		for (int i = 0; i < 10; i++) {
			result = (result + x / result) * 0.5f;
		}
		return result;
	}

	float sin(float x) {
		while (x > PI) x -= 2 * PI;
		while (x < -PI) x += 2 * PI;
		float x3 = x * x * x;
		float x5 = x3 * x * x;
		return x - x3 / 6.0f + x5 / 120.0f;
	}

	float cos(float x) {
		while (x > PI) x -= 2 * PI;
		while (x < -PI) x += 2 * PI;
		float x2 = x * x;
		float x4 = x2 * x2;
		return 1.0f - x2 / 2.0f + x4 / 24.0f;
	}

	float atan2(float y, float x) {
		if (abs(x) < 0.001f) return y > 0 ? PI / 2 : -PI / 2;
		float z = y / x;
		float atan = z / (1.0f + 0.28f * z * z);
		if (x < 0) atan += (y >= 0 ? PI : -PI);
		return atan;
	}
}

// ============================================================================
// BRESENHAM LINE DRAWING
// ============================================================================
void drawPixel(sf::RenderWindow& win, int x, int y, sf::Color c) {
	if (x >= 0 && x < 1200 && y >= 0 && y < 700) {
		sf::Vertex v(sf::Vector2f(x, y), c);
		win.draw(&v, 1, sf::Points);
	}
}

void drawLine(sf::RenderWindow& win, int x1, int y1, int x2, int y2, sf::Color c) {
	int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
	int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
	int err = dx + dy;

	while (true) {
		drawPixel(win, x1, y1, c);
		if (x1 == x2 && y1 == y2) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x1 += sx; }
		if (e2 <= dx) { err += dx; y1 += sy; }
	}
}

void drawCircle(sf::RenderWindow& win, int cx, int cy, int r, sf::Color c, bool fill) {
	int x = 0, y = r;
	int d = 3 - 2 * r;

	while (y >= x) {
		if (fill) {
			for (int i = cx - x; i <= cx + x; i++) {
				drawPixel(win, i, cy + y, c);
				drawPixel(win, i, cy - y, c);
			}
			for (int i = cx - y; i <= cx + y; i++) {
				drawPixel(win, i, cy + x, c);
				drawPixel(win, i, cy - x, c);
			}
		}
		else {
			drawPixel(win, cx + x, cy + y, c);
			drawPixel(win, cx - x, cy + y, c);
			drawPixel(win, cx + x, cy - y, c);
			drawPixel(win, cx - x, cy - y, c);
			drawPixel(win, cx + y, cy + x, c);
			drawPixel(win, cx - y, cy + x, c);
			drawPixel(win, cx + y, cy - x, c);
			drawPixel(win, cx - y, cy - x, c);
		}
		x++;
		if (d > 0) {
			y--;
			d += 4 * (x - y) + 10;
		}
		else {
			d += 4 * x + 6;
		}
	}
}

void fillRect(sf::RenderWindow& win, int x, int y, int w, int h, sf::Color c) {
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			drawPixel(win, x + j, y + i, c);
		}
	}
}

// ============================================================================
// TERRAIN - User can draw their own hills!
// ============================================================================
class Terrain {
public:
	std::vector<sf::Vector2f> points;
	bool drawing;

	Terrain() : drawing(false) {
		// Default beautiful hill
		points.push_back({ 0, 500 });
		points.push_back({ 150, 450 });
		points.push_back({ 300, 350 });
		points.push_back({ 450, 300 });
		points.push_back({ 600, 400 });
		points.push_back({ 750, 500 });
		points.push_back({ 900, 550 });
		points.push_back({ 1200, 600 });
	}

	void startDrawing(float x, float y) {
		points.clear();
		points.push_back({ 0, y });
		drawing = true;
	}

	void addPoint(float x, float y) {
		if (drawing && points.size() < 100) {
			points.push_back({ x, y });
		}
	}

	void finishDrawing() {
		drawing = false;
		if (!points.empty()) {
			points.push_back({ 1200, points.back().y });
		}
	}

	void draw(sf::RenderWindow& win) {
		// Draw gradient fill
		for (size_t i = 0; i < points.size() - 1; i++) {
			int x1 = points[i].x;
			int x2 = points[i + 1].x;
			int y1 = points[i].y;
			int y2 = points[i + 1].y;

			for (int x = x1; x <= x2; x++) {
				float t = (x - x1) / (float)(x2 - x1 + 1);
				int y = y1 + t * (y2 - y1);

				// Gradient color based on height
				int green = 100 + (700 - y) / 4;
				if (green > 200) green = 200;
				sf::Color grassColor(50, green, 40);

				drawLine(win, x, y, x, 700, grassColor);
			}
		}

		// Draw outline
		for (size_t i = 0; i < points.size() - 1; i++) {
			drawLine(win, points[i].x, points[i].y,
				points[i + 1].x, points[i + 1].y,
				sf::Color(30, 80, 30));
		}
	}

	float getHeight(float x) {
		if (points.empty()) return 600;
		if (x <= points[0].x) return points[0].y;
		if (x >= points.back().x) return points.back().y;

		for (size_t i = 0; i < points.size() - 1; i++) {
			if (x >= points[i].x && x <= points[i + 1].x) {
				float t = (x - points[i].x) / (points[i + 1].x - points[i].x);
				return points[i].y + t * (points[i + 1].y - points[i].y);
			}
		}
		return 600;
	}

	float getSlope(float x) {
		for (size_t i = 0; i < points.size() - 1; i++) {
			if (x >= points[i].x && x <= points[i + 1].x) {
				float dx = points[i + 1].x - points[i].x;
				float dy = points[i + 1].y - points[i].y;
				return Math::atan2(dy, dx);
			}
		}
		return 0;
	}
};

// ============================================================================
// CAR - Beautiful design with realistic physics
// ============================================================================
class Car {
public:
	float x, y;
	float vx, vy;
	float angle;
	float wheelAngle;
	float size;
	bool active;

	Car() : x(100), y(100), vx(0), vy(0), angle(0), wheelAngle(0), size(30), active(false) {}

	void place(float px, float py) {
		x = px;
		y = py;
		vx = vy = 0;
		wheelAngle = 0;
		active = true;
	}

	void update(float dt, Terrain& terrain) {
		if (!active) return;

		const float GRAVITY = 600.0f;
		const float FRICTION = 0.15f;
		const float DAMPING = 0.995f;

		float groundY = terrain.getHeight(x);
		float slope = terrain.getSlope(x);

		if (y + size >= groundY) {
			// On ground
			y = groundY - size;
			angle = slope;

			// Physics: a = g*sin(θ) - friction*g*cos(θ)
			float sinS = Math::sin(slope);
			float cosS = Math::cos(slope);

			float accel = GRAVITY * sinS - FRICTION * GRAVITY * Math::abs(cosS);

			vx += accel * cosS * dt;
			vy = accel * sinS * dt;

			vx *= DAMPING;

			// Update wheel rotation
			wheelAngle += (vx * dt) / (size * 0.4f);

		}
		else {
			// In air
			vy += GRAVITY * dt;
		}

		x += vx * dt;
		y += vy * dt;

		// Bounds
		if (x < 0) { x = 0; vx = 0; }
		if (x > 1200) { x = 1200; vx = 0; }
	}

	void draw(sf::RenderWindow& win) {
		if (!active) return;

		float cs = Math::cos(angle);
		float sn = Math::sin(angle);

		// Car body dimensions
		float bodyW = size * 2.5f;
		float bodyH = size * 0.8f;
		float wheelR = size * 0.4f;

		// Wheels positions
		float wheel1X = x - bodyW * 0.3f * cs;
		float wheel1Y = y - bodyW * 0.3f * sn;
		float wheel2X = x + bodyW * 0.3f * cs;
		float wheel2Y = y + bodyW * 0.3f * sn;

		// Draw wheels
		drawWheel(win, wheel1X, wheel1Y, wheelR);
		drawWheel(win, wheel2X, wheel2Y, wheelR);

		// Draw car body (rotated rectangle)
		drawRotatedRect(win, x, y - size * 0.5f, bodyW, bodyH, angle, sf::Color(220, 50, 50));

		// Draw cabin
		float cabinW = bodyW * 0.5f;
		float cabinH = bodyH * 0.8f;
		drawRotatedRect(win, x, y - size * 0.9f, cabinW, cabinH, angle, sf::Color(180, 40, 40));

		// Windows
		drawRotatedRect(win, x - cabinW * 0.15f * cs, y - size * 0.9f - cabinW * 0.15f * sn,
			cabinW * 0.35f, cabinH * 0.6f, angle, sf::Color(100, 150, 200));
		drawRotatedRect(win, x + cabinW * 0.15f * cs, y - size * 0.9f + cabinW * 0.15f * sn,
			cabinW * 0.35f, cabinH * 0.6f, angle, sf::Color(100, 150, 200));
	}

private:
	void drawWheel(sf::RenderWindow& win, float cx, float cy, float r) {
		// Tire
		drawCircle(win, cx, cy, r, sf::Color(40, 40, 40), true);

		// Rim
		drawCircle(win, cx, cy, r * 0.6f, sf::Color(150, 150, 150), true);

		// Spokes showing rotation
		for (int i = 0; i < 6; i++) {
			float a = wheelAngle + i * Math::PI / 3;
			float dx = r * 0.5f * Math::cos(a);
			float dy = r * 0.5f * Math::sin(a);
			drawLine(win, cx, cy, cx + dx, cy + dy, sf::Color(80, 80, 80));
		}
	}

	void drawRotatedRect(sf::RenderWindow& win, float cx, float cy,
		float w, float h, float ang, sf::Color col) {
		float cs = Math::cos(ang);
		float sn = Math::sin(ang);
		float hw = w / 2, hh = h / 2;

		int x1 = cx + (-hw * cs - (-hh) * sn);
		int y1 = cy + (-hw * sn + (-hh) * cs);
		int x2 = cx + (hw * cs - (-hh) * sn);
		int y2 = cy + (hw * sn + (-hh) * cs);
		int x3 = cx + (hw * cs - hh * sn);
		int y3 = cy + (hw * sn + hh * cs);
		int x4 = cx + (-hw * cs - hh * sn);
		int y4 = cy + (-hw * sn + hh * cs);

		// Fill
		int minY = y1, maxY = y1;
		if (y2 < minY) minY = y2; if (y2 > maxY) maxY = y2;
		if (y3 < minY) minY = y3; if (y3 > maxY) maxY = y3;
		if (y4 < minY) minY = y4; if (y4 > maxY) maxY = y4;

		for (int y = minY; y <= maxY; y++) {
			for (int x = cx - w; x <= cx + w; x++) {
				float localX = (x - cx) * cs + (y - cy) * sn;
				float localY = -(x - cx) * sn + (y - cy) * cs;
				if (Math::abs(localX) <= hw && Math::abs(localY) <= hh) {
					drawPixel(win, x, y, col);
				}
			}
		}

		// Outline
		drawLine(win, x1, y1, x2, y2, sf::Color::Black);
		drawLine(win, x2, y2, x3, y3, sf::Color::Black);
		drawLine(win, x3, y3, x4, y4, sf::Color::Black);
		drawLine(win, x4, y4, x1, y1, sf::Color::Black);
	}
};

// ============================================================================
// MAIN
// ============================================================================
int main() {
	sf::RenderWindow window(sf::VideoMode(1200, 700), "Car Rolling Physics - Click to Place Car!");
	window.setFramerateLimit(60);

	Terrain terrain;
	Car car;

	sf::Font font;
	sf::Text instructions;
	instructions.setCharacterSize(18);
	instructions.setFillColor(sf::Color::White);
	instructions.setPosition(10, 10);

	bool showHelp = true;

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();

			if (event.type == sf::Event::KeyPressed) {
				if (event.key.code == sf::Keyboard::R) {
					car = Car();
					showHelp = true;
				}
				if (event.key.code == sf::Keyboard::D) {
					terrain.startDrawing(event.mouseMove.x, event.mouseMove.y);
				}
				if (event.key.code == sf::Keyboard::H) {
					showHelp = !showHelp;
				}
				if (event.key.code == sf::Keyboard::Space) {
					terrain = Terrain(); // Reset to default hill
				}
			}

			if (event.type == sf::Event::MouseButtonPressed) {
				if (event.mouseButton.button == sf::Mouse::Left) {
					if (terrain.drawing) {
						terrain.finishDrawing();
					}
					else {
						float mx = event.mouseButton.x;
						float my = event.mouseButton.y;
						car.place(mx, my);
						showHelp = false;
					}
				}
			}

			if (event.type == sf::Event::MouseMoved && terrain.drawing) {
				terrain.addPoint(event.mouseMove.x, event.mouseMove.y);
			}
		}

		// Update
		car.update(1.0f / 60.0f, terrain);

		// Render
		window.clear(sf::Color(135, 206, 250)); // Sky blue

		terrain.draw(window);
		car.draw(window);

		// Instructions
		if (showHelp) {
			instructions.setString(
				"CLICK anywhere to place the car!\n"
				"SPACE = Reset hill to default\n"
				"R = Reset car\n"
				"H = Hide help\n"
				"Watch realistic physics in action!"
			);
			window.draw(instructions);
		}

		window.display();
	}

	return 0;
}