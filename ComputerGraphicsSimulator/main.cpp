#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

struct Line {
	sf::Vector2f p1;
	sf::Vector2f p2;
	sf::Color color;
	bool useDDA; // true = DDA, false = Bresenham
};

// --- Draw Functions ---
void drawDDALine(std::vector<sf::RectangleShape>& pixels, sf::Vector2f p1, sf::Vector2f p2, sf::Color color) {
	float dx = p2.x - p1.x;
	float dy = p2.y - p1.y;
	float steps = std::max(std::abs(dx), std::abs(dy));
	float xInc = dx / steps;
	float yInc = dy / steps;
	float x = p1.x, y = p1.y;
	for (int i = 0; i <= steps; i++) {
		sf::RectangleShape r(sf::Vector2f(2, 2));
		r.setPosition(std::round(x), std::round(y));
		r.setFillColor(color);
		pixels.push_back(r);
		x += xInc;
		y += yInc;
	}
}

void drawBresenhamLine(std::vector<sf::RectangleShape>& pixels, sf::Vector2f p1, sf::Vector2f p2, sf::Color color) {
	int x1 = (int)std::round(p1.x);
	int y1 = (int)std::round(p1.y);
	int x2 = (int)std::round(p2.x);
	int y2 = (int)std::round(p2.y);

	int dx = std::abs(x2 - x1);
	int dy = std::abs(y2 - y1);
	int sx = (x1 < x2) ? 1 : -1;
	int sy = (y1 < y2) ? 1 : -1;
	int err = dx - dy;

	while (true) {
		sf::RectangleShape r(sf::Vector2f(2, 2));
		r.setPosition((float)x1, (float)y1);
		r.setFillColor(color);
		pixels.push_back(r);

		if (x1 == x2 && y1 == y2) break;

		int e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x1 += sx; }
		if (e2 < dx) { err += dx; y1 += sy; }
	}
}

// --- Translate Line ---
void translateLine(Line& line, float dx, float dy) {
	line.p1.x += dx; line.p1.y += dy;
	line.p2.x += dx; line.p2.y += dy;
}

// --- Rotate Line around its center ---
void rotateLine(Line& line, float angleDegrees) {
	sf::Vector2f center = (line.p1 + line.p2) / 2.0f;
	float angle = angleDegrees * 3.14159265f / 180.0f; // convert to radians

	auto rotatePoint = [&](sf::Vector2f& p) {
		float x = p.x - center.x;
		float y = p.y - center.y;
		float xr = x * cos(angle) - y * sin(angle);
		float yr = x * sin(angle) + y * cos(angle);
		p.x = center.x + xr;
		p.y = center.y + yr;
		};

	rotatePoint(line.p1);
	rotatePoint(line.p2);
}

// --- Scale Line from center ---
void scaleLine(Line& line, float factor) {
	sf::Vector2f center = (line.p1 + line.p2) / 2.0f;

	auto scalePoint = [&](sf::Vector2f& p) {
		p = center + (p - center) * factor;
		};

	scalePoint(line.p1);
	scalePoint(line.p2);
}

// --- Distance from point to line ---
float distancePointToLine(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b) {
	float A = p.x - a.x;
	float B = p.y - a.y;
	float C = b.x - a.x;
	float D = b.y - a.y;
	float dot = A * C + B * D;
	float len_sq = C * C + D * D;
	float param = (len_sq != 0) ? dot / len_sq : -1;

	float xx, yy;
	if (param < 0) { xx = a.x; yy = a.y; }
	else if (param > 1) { xx = b.x; yy = b.y; }
	else { xx = a.x + param * C; yy = a.y + param * D; }

	float dx = p.x - xx;
	float dy = p.y - yy;
	return std::sqrt(dx * dx + dy * dy);
}

int main() {
	sf::RenderWindow window(sf::VideoMode(1000, 700), "Mini-CAD: Line Editor with Transformations");

	std::vector<Line> lines;
	std::vector<sf::Vector2f> tempPoints;

	enum Mode { SELECTION = 1, DRAW_DDA = 2, DRAW_BRES = 3 };
	Mode currentMode = SELECTION; // default mode = selection

	int selectedLine = -1;
	const float moveAmount = 0.50f;
	const float rotateAmount = 0.1f;   // degrees per key press
	const float scaleFactorUp = 1.001f;
	const float scaleFactorDown = 0.998f;

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();

			// --- Mouse click ---
			if (event.type == sf::Event::MouseButtonPressed) {
				sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

				// Selection Mode
				if (currentMode == SELECTION && event.mouseButton.button == sf::Mouse::Left) {
					selectedLine = -1;
					for (size_t i = 0; i < lines.size(); i++) {
						if (distancePointToLine(mousePos, lines[i].p1, lines[i].p2) < 8.0f) {
							selectedLine = (int)i;
							break;
						}
					}
				}

				// Draw Modes
				if ((currentMode == DRAW_DDA || currentMode == DRAW_BRES) && event.mouseButton.button == sf::Mouse::Left) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						Line line;
						line.p1 = tempPoints[0];
						line.p2 = tempPoints[1];
						line.useDDA = (currentMode == DRAW_DDA);
						line.color = line.useDDA ? sf::Color::Red : sf::Color::Green;
						lines.push_back(line);
						tempPoints.clear();
					}
				}
			}

			// --- Keyboard ---
			if (event.type == sf::Event::KeyPressed) {
				switch (event.key.code) {
				case sf::Keyboard::Num1: currentMode = SELECTION; break;
				case sf::Keyboard::Num2: currentMode = DRAW_DDA; break;
				case sf::Keyboard::Num3: currentMode = DRAW_BRES; break;
				case sf::Keyboard::C: lines.clear(); selectedLine = -1; tempPoints.clear(); break;
				}
			}
		}

		// --- Real-time transformations for selected line ---
		if (selectedLine != -1) {
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
				translateLine(lines[selectedLine], -moveAmount, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
				translateLine(lines[selectedLine], moveAmount, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
				translateLine(lines[selectedLine], 0, -moveAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
				translateLine(lines[selectedLine], 0, moveAmount);

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q))
				rotateLine(lines[selectedLine], -rotateAmount); // counterclockwise
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::E))
				rotateLine(lines[selectedLine], rotateAmount); // clockwise

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::W))
				scaleLine(lines[selectedLine], scaleFactorUp); // increase length
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
				scaleLine(lines[selectedLine], scaleFactorDown); // decrease length
		}

		// --- Draw ---
		window.clear(sf::Color(30, 30, 30));
		std::vector<sf::RectangleShape> pixels;

		for (auto& line : lines) {
			if (line.useDDA) drawDDALine(pixels, line.p1, line.p2, line.color);
			else drawBresenhamLine(pixels, line.p1, line.p2, line.color);
		}

		for (auto& r : pixels) window.draw(r);

		// Highlight selected line on top
		if (selectedLine != -1) {
			auto& line = lines[selectedLine];
			sf::Vertex highlight[2] = {
				sf::Vertex(line.p1, sf::Color::Yellow),
				sf::Vertex(line.p2, sf::Color::Yellow)
			};
			window.draw(highlight, 2, sf::Lines);
		}

		window.display();
	}

	return 0;
}
