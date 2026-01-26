#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

// --- Shape Structs ---
struct Line {
	sf::Vector2f p1;
	sf::Vector2f p2;
	sf::Color color;
	bool useDDA; // true = DDA, false = Bresenham
};

struct Circle {
	sf::Vector2f center;
	float radius;
	sf::Color color;
};

struct Ellipse {
	sf::Vector2f center;
	float rx, ry;
	sf::Color color;
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

// --- Circle Drawing (Midpoint Circle Algorithm) ---
void drawCircle(std::vector<sf::RectangleShape>& pixels, Circle c) {
	int x = 0;
	int y = (int)c.radius;
	int d = 1 - y;

	auto plotCirclePoints = [&](int x, int y) {
		std::vector<sf::Vector2f> pts = {
			{c.center.x + x, c.center.y + y},
			{c.center.x - x, c.center.y + y},
			{c.center.x + x, c.center.y - y},
			{c.center.x - x, c.center.y - y},
			{c.center.x + y, c.center.y + x},
			{c.center.x - y, c.center.y + x},
			{c.center.x + y, c.center.y - x},
			{c.center.x - y, c.center.y - x}
		};
		for (auto& p : pts) {
			sf::RectangleShape r(sf::Vector2f(2, 2));
			r.setPosition(p);
			r.setFillColor(c.color);
			pixels.push_back(r);
		}
		};

	while (x <= y) {
		plotCirclePoints(x, y);
		if (d < 0) d += 2 * x + 3;
		else { d += 2 * (x - y) + 5; y--; }
		x++;
	}
}

// --- Ellipse Drawing (Midpoint Ellipse Algorithm) ---
void drawEllipse(std::vector<sf::RectangleShape>& pixels, Ellipse e) {
	float rx2 = e.rx * e.rx;
	float ry2 = e.ry * e.ry;
	float x = 0, y = e.ry;
	float dx = 2 * ry2 * x;
	float dy = 2 * rx2 * y;

	auto plotEllipsePoints = [&](float x, float y) {
		std::vector<sf::Vector2f> pts = {
			{e.center.x + x, e.center.y + y},
			{e.center.x - x, e.center.y + y},
			{e.center.x + x, e.center.y - y},
			{e.center.x - x, e.center.y - y}
		};
		for (auto& p : pts) {
			sf::RectangleShape r(sf::Vector2f(2, 2));
			r.setPosition(p);
			r.setFillColor(e.color);
			pixels.push_back(r);
		}
		};

	// Region 1
	float p1 = ry2 - (rx2 * e.ry) + (0.25f * rx2);
	while (dx < dy) {
		plotEllipsePoints(x, y);
		if (p1 < 0) { x++; dx += 2 * ry2; p1 += dx + ry2; }
		else { x++; y--; dx += 2 * ry2; dy -= 2 * rx2; p1 += dx - dy + ry2; }
	}

	// Region 2
	float p2 = ry2 * (x + 0.5f) * (x + 0.5f) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
	while (y >= 0) {
		plotEllipsePoints(x, y);
		if (p2 > 0) { y--; dy -= 2 * rx2; p2 += rx2 - dy; }
		else { y--; x++; dx += 2 * ry2; dy -= 2 * rx2; p2 += dx - dy + rx2; }
	}
}

// --- Transformations ---
void translateLine(Line& line, float dx, float dy) { line.p1.x += dx; line.p1.y += dy; line.p2.x += dx; line.p2.y += dy; }
void rotateLine(Line& line, float angleDegrees) {
	sf::Vector2f center = (line.p1 + line.p2) / 2.0f;
	float angle = angleDegrees * 3.14159265f / 180.0f;
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
void scaleLine(Line& line, float factor) {
	sf::Vector2f center = (line.p1 + line.p2) / 2.0f;
	auto scalePoint = [&](sf::Vector2f& p) { p = center + (p - center) * factor; };
	scalePoint(line.p1);
	scalePoint(line.p2);
}

void translateCircle(Circle& c, float dx, float dy) { c.center.x += dx; c.center.y += dy; }
void scaleCircle(Circle& c, float factor) { c.radius *= factor; }

void translateEllipse(Ellipse& e, float dx, float dy) { e.center.x += dx; e.center.y += dy; }
void scaleEllipse(Ellipse& e, float factor) { e.rx *= factor; e.ry *= factor; }

// --- Distance from point to line segment ---
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
	sf::RenderWindow window(sf::VideoMode(1000, 700), "Mini-CAD: Line, Circle, Ellipse Editor");

	std::vector<Line> lines;
	std::vector<Circle> circles;
	std::vector<Ellipse> ellipses;
	std::vector<sf::Vector2f> tempPoints;

	enum Mode { SELECTION = 1, DRAW_DDA = 2, DRAW_BRES = 3, DRAW_CIRCLE = 4, DRAW_ELLIPSE = 5 };
	Mode currentMode = SELECTION;

	int selectedLine = -1, selectedCircle = -1, selectedEllipse = -1;
	const float moveAmount = 0.50f;
	const float rotateAmount = 0.5f;   // degrees per key press
	const float scaleFactorUp = 1.01f;
	const float scaleFactorDown = 0.99f;

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed) window.close();

			// --- Keyboard mode switching ---
			if (event.type == sf::Event::KeyPressed) {
				switch (event.key.code) {
				case sf::Keyboard::Num1: currentMode = SELECTION; break;
				case sf::Keyboard::Num2: currentMode = DRAW_DDA; break;
				case sf::Keyboard::Num3: currentMode = DRAW_BRES; break;
				case sf::Keyboard::Num4: currentMode = DRAW_CIRCLE; break;
				case sf::Keyboard::Num5: currentMode = DRAW_ELLIPSE; break;
				case sf::Keyboard::C:
					lines.clear(); circles.clear(); ellipses.clear();
					selectedLine = selectedCircle = selectedEllipse = -1;
					tempPoints.clear();
					break;
				}
			}

			if (event.type == sf::Event::KeyPressed) {
				if (event.key.code == sf::Keyboard::Delete) {
					if (selectedLine != -1) {
						lines.erase(lines.begin() + selectedLine);
						selectedLine = -1;
					}
					else if (selectedCircle != -1) {
						circles.erase(circles.begin() + selectedCircle);
						selectedCircle = -1;
					}
					else if (selectedEllipse != -1) {
						ellipses.erase(ellipses.begin() + selectedEllipse);
						selectedEllipse = -1;
					}
				}
			}

			// --- Mouse click ---
			if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
				sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

				// Selection Mode
				if (currentMode == SELECTION) {
					selectedLine = selectedCircle = selectedEllipse = -1;

					// Check lines
					for (size_t i = 0; i < lines.size(); i++) {
						if (distancePointToLine(mousePos, lines[i].p1, lines[i].p2) < 8.0f) {
							selectedLine = (int)i; break;
						}
					}
					//Check circles
					for (size_t i = 0; i < circles.size(); i++) {
						float dx = mousePos.x - circles[i].center.x;
						float dy = mousePos.y - circles[i].center.y;
						float dist = std::sqrt(dx * dx + dy * dy);

						// Edge-only selection
						if (std::abs(dist - circles[i].radius) < 8.0f) {
							selectedCircle = (int)i;
							break;
						}

						// Or: interior selection (more intuitive)
						// if (dist <= circles[i].radius) { selectedCircle = (int)i; break; }
					}
					// Check ellipses
/// Check ellipses - edge-only selection
					for (size_t i = 0; i < ellipses.size(); i++) {
						float dx = mousePos.x - ellipses[i].center.x;
						float dy = mousePos.y - ellipses[i].center.y;

						float rx = ellipses[i].rx;
						float ry = ellipses[i].ry;

						// Normalized ellipse equation
						float val = (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry);

						// Edge-only: select if close to boundary (≈1.0)
						if (std::abs(val - 1.0f) < 0.05f) {   // tolerance can be tuned
							selectedEllipse = static_cast<int>(i);
							break;
						}
					}
				}

				// Draw Line
				// Draw Line (DDA or Bresenham depending on mode)
				// --- Draw Line (DDA) ---
				if (currentMode == DRAW_DDA) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						Line l;
						l.p1 = tempPoints[0];
						l.p2 = tempPoints[1];
						l.color = sf::Color::Green;   // DDA lines = Green
						l.useDDA = true;
						lines.push_back(l);
						tempPoints.clear();
					}
				}

				// --- Draw Line (Bresenham) ---
				if (currentMode == DRAW_BRES) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						Line l;
						l.p1 = tempPoints[0];
						l.p2 = tempPoints[1];
						l.color = sf::Color::Red;     // Bresenham lines = Red
						l.useDDA = false;
						lines.push_back(l);
						tempPoints.clear();
					}
				}

				// Draw Circle
				if (currentMode == DRAW_CIRCLE) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						Circle c;
						c.center = tempPoints[0];
						float dx = tempPoints[1].x - tempPoints[0].x;
						float dy = tempPoints[1].y - tempPoints[0].y;
						c.radius = std::sqrt(dx * dx + dy * dy);
						c.color = sf::Color::Blue;
						circles.push_back(c);
						tempPoints.clear();
					}
				}

				// Draw Ellipse
				if (currentMode == DRAW_ELLIPSE) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						Ellipse e;
						e.center = tempPoints[0];
						e.rx = std::abs(tempPoints[1].x - tempPoints[0].x);
						e.ry = std::abs(tempPoints[1].y - tempPoints[0].y);
						e.color = sf::Color::Magenta;
						ellipses.push_back(e);
						tempPoints.clear();
					}
				}
			}
		}

		// --- Real-time transformations ---
		if (selectedLine != -1) {
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) translateLine(lines[selectedLine], -moveAmount, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) translateLine(lines[selectedLine], moveAmount, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) translateLine(lines[selectedLine], 0, -moveAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) translateLine(lines[selectedLine], 0, moveAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) rotateLine(lines[selectedLine], -rotateAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::E)) rotateLine(lines[selectedLine], rotateAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) scaleLine(lines[selectedLine], scaleFactorUp);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) scaleLine(lines[selectedLine], scaleFactorDown);
		}
		if (selectedCircle != -1) {
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) translateCircle(circles[selectedCircle], -moveAmount, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) translateCircle(circles[selectedCircle], moveAmount, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) translateCircle(circles[selectedCircle], 0, -moveAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) translateCircle(circles[selectedCircle], 0, moveAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) scaleCircle(circles[selectedCircle], scaleFactorUp);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) scaleCircle(circles[selectedCircle], scaleFactorDown);
		}
		if (selectedEllipse != -1) {
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) translateEllipse(ellipses[selectedEllipse], -moveAmount, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) translateEllipse(ellipses[selectedEllipse], moveAmount, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) translateEllipse(ellipses[selectedEllipse], 0, -moveAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) translateEllipse(ellipses[selectedEllipse], 0, moveAmount);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) scaleEllipse(ellipses[selectedEllipse], scaleFactorUp);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) scaleEllipse(ellipses[selectedEllipse], scaleFactorDown);
		}

		// --- Draw ---
		window.clear(sf::Color(30, 30, 30));
		std::vector<sf::RectangleShape> pixels;

		for (auto& line : lines) {
			if (line.useDDA) drawDDALine(pixels, line.p1, line.p2, line.color);
			else drawBresenhamLine(pixels, line.p1, line.p2, line.color);
		}
		for (auto& c : circles) drawCircle(pixels, c);
		for (auto& e : ellipses) drawEllipse(pixels, e);

		for (auto& r : pixels) window.draw(r);

		// Highlight selected shapes
		if (selectedLine != -1) {
			auto& line = lines[selectedLine];
			sf::Vertex highlight[2] = {
				sf::Vertex(line.p1, sf::Color::Yellow),
				sf::Vertex(line.p2, sf::Color::Yellow)
			};
			window.draw(highlight, 2, sf::Lines);
		}
		 if (selectedCircle != -1) {
            auto& c = circles[selectedCircle];
            sf::CircleShape highlight(c.radius);
            // CircleShape position is top-left of bounding box, so offset by radius
            highlight.setPosition(c.center.x - c.radius, c.center.y - c.radius);
            highlight.setFillColor(sf::Color::Transparent);
            highlight.setOutlineColor(sf::Color::Yellow);
            highlight.setOutlineThickness(2);
            window.draw(highlight);
        }

		 if (selectedEllipse != -1) {
			 Ellipse e = ellipses[selectedEllipse];
			 Ellipse highlight = e;
			 highlight.color = sf::Color::Yellow; // override color
			 drawEllipse(pixels, highlight);      // same algorithm, now yellow
		 }

        window.display();
    }

    return 0;
}
