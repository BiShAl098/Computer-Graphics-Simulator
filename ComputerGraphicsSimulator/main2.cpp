#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>

// ============================================================================
// CONSTANTS AND CONFIGURATION
// ============================================================================
const float SELECTION_THRESHOLD = 10.0f;
const float MOVE_AMOUNT = 0.75f;
const float ROTATE_AMOUNT = 1.0f;
const float SCALE_FACTOR_UP = 1.02f;
const float SCALE_FACTOR_DOWN = 0.98f;
const float PI = 3.14159265358979323846f;
const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 800;

// ============================================================================
// UTILITY STRUCTURES
// ============================================================================
struct Transform2D {
	float tx, ty;      // Translation
	float rotation;    // Rotation in radians
	float sx, sy;      // Scale

	Transform2D() : tx(0), ty(0), rotation(0), sx(1), sy(1) {}
};

// ============================================================================
// BASE SHAPE CLASS (POLYMORPHIC DESIGN)
// ============================================================================
class Shape {
public:
	sf::Color color;
	bool isSelected;
	Transform2D transform;

	Shape() : color(sf::Color::White), isSelected(false) {}
	virtual ~Shape() = default;

	virtual void draw(std::vector<sf::RectangleShape>& pixels) = 0;
	virtual bool containsPoint(sf::Vector2f point) = 0;
	virtual void translate(float dx, float dy) = 0;
	virtual void rotate(float angleDegrees) = 0;
	virtual void scale(float factor) = 0;
	virtual sf::Vector2f getCenter() = 0;
	virtual std::string getInfo() = 0;
};

// ============================================================================
// MATRIX OPERATIONS FOR TRANSFORMATIONS
// ============================================================================
struct Matrix3x3 {
	float m[3][3];

	Matrix3x3() {
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				m[i][j] = (i == j) ? 1.0f : 0.0f;
	}

	static Matrix3x3 translation(float tx, float ty) {
		Matrix3x3 mat;
		mat.m[0][2] = tx;
		mat.m[1][2] = ty;
		return mat;
	}

	static Matrix3x3 rotation(float angleRad) {
		Matrix3x3 mat;
		mat.m[0][0] = cos(angleRad);
		mat.m[0][1] = -sin(angleRad);
		mat.m[1][0] = sin(angleRad);
		mat.m[1][1] = cos(angleRad);
		return mat;
	}

	static Matrix3x3 scaling(float sx, float sy) {
		Matrix3x3 mat;
		mat.m[0][0] = sx;
		mat.m[1][1] = sy;
		return mat;
	}

	sf::Vector2f transform(sf::Vector2f p) const {
		float x = m[0][0] * p.x + m[0][1] * p.y + m[0][2];
		float y = m[1][0] * p.x + m[1][1] * p.y + m[1][2];
		return sf::Vector2f(x, y);
	}

	Matrix3x3 multiply(const Matrix3x3& other) const {
		Matrix3x3 result;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				result.m[i][j] = 0;
				for (int k = 0; k < 3; k++) {
					result.m[i][j] += m[i][k] * other.m[k][j];
				}
			}
		}
		return result;
	}
};

// ============================================================================
// LINE CLASS
// ============================================================================
class Line : public Shape {
public:
	sf::Vector2f p1, p2;
	enum Algorithm { DDA, BRESENHAM } algorithm;

	Line(sf::Vector2f start, sf::Vector2f end, Algorithm algo, sf::Color col)
		: p1(start), p2(end), algorithm(algo) {
		color = col;
	}

	void draw(std::vector<sf::RectangleShape>& pixels) override {
		if (algorithm == DDA) {
			drawDDA(pixels);
		}
		else {
			drawBresenham(pixels);
		}
	}

	bool containsPoint(sf::Vector2f point) override {
		return distanceToSegment(point) < SELECTION_THRESHOLD;
	}

	void translate(float dx, float dy) override {
		p1.x += dx; p1.y += dy;
		p2.x += dx; p2.y += dy;
	}

	void rotate(float angleDegrees) override {
		sf::Vector2f center = getCenter();
		float angleRad = angleDegrees * PI / 180.0f;

		// Manual rotation calculation
		auto rotatePoint = [&](sf::Vector2f& p) {
			float dx = p.x - center.x;
			float dy = p.y - center.y;
			float xNew = dx * cos(angleRad) - dy * sin(angleRad);
			float yNew = dx * sin(angleRad) + dy * cos(angleRad);
			p.x = center.x + xNew;
			p.y = center.y + yNew;
			};

		rotatePoint(p1);
		rotatePoint(p2);
	}

	void scale(float factor) override {
		sf::Vector2f center = getCenter();
		p1 = center + (p1 - center) * factor;
		p2 = center + (p2 - center) * factor;
	}

	sf::Vector2f getCenter() override {
		return (p1 + p2) / 2.0f;
	}

	std::string getInfo() override {
		std::stringstream ss;
		float length = sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
		ss << "Line (" << (algorithm == DDA ? "DDA" : "Bresenham") << ") | "
			<< "Length: " << std::fixed << std::setprecision(1) << length;
		return ss.str();
	}

private:
	void drawDDA(std::vector<sf::RectangleShape>& pixels) {
		float dx = p2.x - p1.x;
		float dy = p2.y - p1.y;
		float steps = std::max(std::abs(dx), std::abs(dy));

		if (steps == 0) return;

		float xInc = dx / steps;
		float yInc = dy / steps;
		float x = p1.x;
		float y = p1.y;

		for (int i = 0; i <= steps; i++) {
			sf::RectangleShape pixel(sf::Vector2f(2, 2));
			pixel.setPosition(std::round(x), std::round(y));
			pixel.setFillColor(color);
			pixels.push_back(pixel);
			x += xInc;
			y += yInc;
		}
	}

	void drawBresenham(std::vector<sf::RectangleShape>& pixels) {
		int x1 = static_cast<int>(std::round(p1.x));
		int y1 = static_cast<int>(std::round(p1.y));
		int x2 = static_cast<int>(std::round(p2.x));
		int y2 = static_cast<int>(std::round(p2.y));

		int dx = std::abs(x2 - x1);
		int dy = std::abs(y2 - y1);
		int sx = (x1 < x2) ? 1 : -1;
		int sy = (y1 < y2) ? 1 : -1;
		int err = dx - dy;

		while (true) {
			sf::RectangleShape pixel(sf::Vector2f(2, 2));
			pixel.setPosition(static_cast<float>(x1), static_cast<float>(y1));
			pixel.setFillColor(color);
			pixels.push_back(pixel);

			if (x1 == x2 && y1 == y2) break;

			int e2 = 2 * err;
			if (e2 > -dy) {
				err -= dy;
				x1 += sx;
			}
			if (e2 < dx) {
				err += dx;
				y1 += sy;
			}
		}
	}

	float distanceToSegment(sf::Vector2f p) {
		float dx = p2.x - p1.x;
		float dy = p2.y - p1.y;
		float lengthSquared = dx * dx + dy * dy;

		if (lengthSquared == 0) {
			dx = p.x - p1.x;
			dy = p.y - p1.y;
			return sqrt(dx * dx + dy * dy);
		}

		float t = ((p.x - p1.x) * dx + (p.y - p1.y) * dy) / lengthSquared;
		t = std::max(0.0f, std::min(1.0f, t));

		float projX = p1.x + t * dx;
		float projY = p1.y + t * dy;

		dx = p.x - projX;
		dy = p.y - projY;
		return sqrt(dx * dx + dy * dy);
	}
};

// ============================================================================
// CIRCLE CLASS (MIDPOINT CIRCLE ALGORITHM)
// ============================================================================
class Circle : public Shape {
public:
	sf::Vector2f center;
	float radius;

	Circle(sf::Vector2f c, float r, sf::Color col)
		: center(c), radius(r) {
		color = col;
	}

	void draw(std::vector<sf::RectangleShape>& pixels) override {
		// Midpoint Circle Algorithm
		int x = 0;
		int y = static_cast<int>(radius);
		int d = 1 - y;

		while (x <= y) {
			plot8Points(pixels, x, y);

			if (d < 0) {
				d += 2 * x + 3;
			}
			else {
				d += 2 * (x - y) + 5;
				y--;
			}
			x++;
		}
	}

	bool containsPoint(sf::Vector2f point) override {
		float dx = point.x - center.x;
		float dy = point.y - center.y;
		float distance = sqrt(dx * dx + dy * dy);
		return std::abs(distance - radius) < SELECTION_THRESHOLD;
	}

	void translate(float dx, float dy) override {
		center.x += dx;
		center.y += dy;
	}

	void rotate(float angleDegrees) override {
		// Circle is rotationally symmetric, no visual change
	}

	void scale(float factor) override {
		radius *= factor;
		if (radius < 1.0f) radius = 1.0f;
	}

	sf::Vector2f getCenter() override {
		return center;
	}

	std::string getInfo() override {
		std::stringstream ss;
		float area = PI * radius * radius;
		float circumference = 2 * PI * radius;
		ss << "Circle | Radius: " << std::fixed << std::setprecision(1) << radius
			<< " | Area: " << area << " | Circum: " << circumference;
		return ss.str();
	}

private:
	void plot8Points(std::vector<sf::RectangleShape>& pixels, int x, int y) {
		std::vector<sf::Vector2f> points = {
			{center.x + x, center.y + y}, {center.x - x, center.y + y},
			{center.x + x, center.y - y}, {center.x - x, center.y - y},
			{center.x + y, center.y + x}, {center.x - y, center.y + x},
			{center.x + y, center.y - x}, {center.x - y, center.y - x}
		};

		for (auto& p : points) {
			sf::RectangleShape pixel(sf::Vector2f(2, 2));
			pixel.setPosition(p);
			pixel.setFillColor(color);
			pixels.push_back(pixel);
		}
	}
};

// ============================================================================
// ELLIPSE CLASS (MIDPOINT ELLIPSE ALGORITHM)
// ============================================================================
class Ellipse : public Shape {
public:
	sf::Vector2f center;
	float rx, ry;

	Ellipse(sf::Vector2f c, float radiusX, float radiusY, sf::Color col)
		: center(c), rx(radiusX), ry(radiusY) {
		color = col;
		if (rx < 1.0f) rx = 1.0f;
		if (ry < 1.0f) ry = 1.0f;
	}

	void draw(std::vector<sf::RectangleShape>& pixels) override {
		// Midpoint Ellipse Algorithm
		float rx2 = rx * rx;
		float ry2 = ry * ry;
		float x = 0, y = ry;
		float dx = 2 * ry2 * x;
		float dy = 2 * rx2 * y;

		// Region 1: slope > -1
		float p1 = ry2 - (rx2 * ry) + (0.25f * rx2);
		while (dx < dy) {
			plot4Points(pixels, x, y);

			if (p1 < 0) {
				x++;
				dx += 2 * ry2;
				p1 += dx + ry2;
			}
			else {
				x++;
				y--;
				dx += 2 * ry2;
				dy -= 2 * rx2;
				p1 += dx - dy + ry2;
			}
		}

		// Region 2: slope < -1
		float p2 = ry2 * (x + 0.5f) * (x + 0.5f) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
		while (y >= 0) {
			plot4Points(pixels, x, y);

			if (p2 > 0) {
				y--;
				dy -= 2 * rx2;
				p2 += rx2 - dy;
			}
			else {
				y--;
				x++;
				dx += 2 * ry2;
				dy -= 2 * rx2;
				p2 += dx - dy + rx2;
			}
		}
	}

	bool containsPoint(sf::Vector2f point) override {
		float dx = point.x - center.x;
		float dy = point.y - center.y;
		float value = (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry);
		return std::abs(value - 1.0f) < 0.15f;
	}

	void translate(float dx, float dy) override {
		center.x += dx;
		center.y += dy;
	}

	void rotate(float angleDegrees) override {
		// For axis-aligned ellipse, rotation would require storing angle
		// Advanced: could store rotation angle and apply during drawing
	}

	void scale(float factor) override {
		rx *= factor;
		ry *= factor;
		if (rx < 1.0f) rx = 1.0f;
		if (ry < 1.0f) ry = 1.0f;
	}

	sf::Vector2f getCenter() override {
		return center;
	}

	std::string getInfo() override {
		std::stringstream ss;
		float area = PI * rx * ry;
		ss << "Ellipse | Rx: " << std::fixed << std::setprecision(1) << rx
			<< " | Ry: " << ry << " | Area: " << area;
		return ss.str();
	}

private:
	void plot4Points(std::vector<sf::RectangleShape>& pixels, float x, float y) {
		std::vector<sf::Vector2f> points = {
			{center.x + x, center.y + y}, {center.x - x, center.y + y},
			{center.x + x, center.y - y}, {center.x - x, center.y - y}
		};

		for (auto& p : points) {
			sf::RectangleShape pixel(sf::Vector2f(2, 2));
			pixel.setPosition(p);
			pixel.setFillColor(color);
			pixels.push_back(pixel);
		}
	}
};

// ============================================================================
// POLYGON CLASS (SCAN-LINE FILL ALGORITHM)
// ============================================================================
class Polygon : public Shape {
public:
	std::vector<sf::Vector2f> vertices;
	bool filled;

	Polygon(std::vector<sf::Vector2f> verts, sf::Color col, bool fill = false)
		: vertices(verts), filled(fill) {
		color = col;
	}

	void draw(std::vector<sf::RectangleShape>& pixels) override {
		if (vertices.size() < 2) return;

		// Draw edges using Bresenham
		for (size_t i = 0; i < vertices.size(); i++) {
			sf::Vector2f p1 = vertices[i];
			sf::Vector2f p2 = vertices[(i + 1) % vertices.size()];
			drawBresenhamLine(pixels, p1, p2);
		}

		// Fill if needed (scan-line algorithm)
		if (filled && vertices.size() >= 3) {
			scanLineFill(pixels);
		}
	}

	bool containsPoint(sf::Vector2f point) override {
		// Check if point is on any edge
		for (size_t i = 0; i < vertices.size(); i++) {
			sf::Vector2f p1 = vertices[i];
			sf::Vector2f p2 = vertices[(i + 1) % vertices.size()];
			if (distanceToSegment(point, p1, p2) < SELECTION_THRESHOLD) {
				return true;
			}
		}
		return false;
	}

	void translate(float dx, float dy) override {
		for (auto& v : vertices) {
			v.x += dx;
			v.y += dy;
		}
	}

	void rotate(float angleDegrees) override {
		sf::Vector2f center = getCenter();
		float angleRad = angleDegrees * PI / 180.0f;

		for (auto& v : vertices) {
			float dx = v.x - center.x;
			float dy = v.y - center.y;
			float xNew = dx * cos(angleRad) - dy * sin(angleRad);
			float yNew = dx * sin(angleRad) + dy * cos(angleRad);
			v.x = center.x + xNew;
			v.y = center.y + yNew;
		}
	}

	void scale(float factor) override {
		sf::Vector2f center = getCenter();
		for (auto& v : vertices) {
			v = center + (v - center) * factor;
		}
	}

	sf::Vector2f getCenter() override {
		sf::Vector2f sum(0, 0);
		for (auto& v : vertices) {
			sum += v;
		}
		return sum / static_cast<float>(vertices.size());
	}

	std::string getInfo() override {
		std::stringstream ss;
		ss << "Polygon | Vertices: " << vertices.size()
			<< " | " << (filled ? "Filled" : "Outline");
		return ss.str();
	}

private:
	void drawBresenhamLine(std::vector<sf::RectangleShape>& pixels,
		sf::Vector2f p1, sf::Vector2f p2) {
		int x1 = static_cast<int>(std::round(p1.x));
		int y1 = static_cast<int>(std::round(p1.y));
		int x2 = static_cast<int>(std::round(p2.x));
		int y2 = static_cast<int>(std::round(p2.y));

		int dx = std::abs(x2 - x1);
		int dy = std::abs(y2 - y1);
		int sx = (x1 < x2) ? 1 : -1;
		int sy = (y1 < y2) ? 1 : -1;
		int err = dx - dy;

		while (true) {
			sf::RectangleShape pixel(sf::Vector2f(2, 2));
			pixel.setPosition(static_cast<float>(x1), static_cast<float>(y1));
			pixel.setFillColor(color);
			pixels.push_back(pixel);

			if (x1 == x2 && y1 == y2) break;

			int e2 = 2 * err;
			if (e2 > -dy) { err -= dy; x1 += sx; }
			if (e2 < dx) { err += dx; y1 += sy; }
		}
	}

	void scanLineFill(std::vector<sf::RectangleShape>& pixels) {
		if (vertices.empty()) return;

		// Find bounding box
		float minY = vertices[0].y, maxY = vertices[0].y;
		for (auto& v : vertices) {
			minY = std::min(minY, v.y);
			maxY = std::max(maxY, v.y);
		}

		// Scan line algorithm
		for (int y = static_cast<int>(minY); y <= static_cast<int>(maxY); y++) {
			std::vector<float> intersections;

			// Find intersections with edges
			for (size_t i = 0; i < vertices.size(); i++) {
				sf::Vector2f p1 = vertices[i];
				sf::Vector2f p2 = vertices[(i + 1) % vertices.size()];

				if ((p1.y <= y && p2.y > y) || (p2.y <= y && p1.y > y)) {
					float x = p1.x + (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
					intersections.push_back(x);
				}
			}

			std::sort(intersections.begin(), intersections.end());

			// Fill between pairs of intersections
			for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
				for (int x = static_cast<int>(intersections[i]);
					x <= static_cast<int>(intersections[i + 1]); x++) {
					sf::RectangleShape pixel(sf::Vector2f(2, 2));
					pixel.setPosition(static_cast<float>(x), static_cast<float>(y));
					pixel.setFillColor(color);
					pixels.push_back(pixel);
				}
			}
		}
	}

	float distanceToSegment(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b) {
		float dx = b.x - a.x;
		float dy = b.y - a.y;
		float lengthSquared = dx * dx + dy * dy;

		if (lengthSquared == 0) {
			dx = p.x - a.x;
			dy = p.y - a.y;
			return sqrt(dx * dx + dy * dy);
		}

		float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared;
		t = std::max(0.0f, std::min(1.0f, t));

		float projX = a.x + t * dx;
		float projY = a.y + t * dy;

		dx = p.x - projX;
		dy = p.y - projY;
		return sqrt(dx * dx + dy * dy);
	}
};

// ============================================================================
// BEZIER CURVE CLASS (PARAMETRIC CURVES)
// ============================================================================
class BezierCurve : public Shape {
public:
	std::vector<sf::Vector2f> controlPoints;
	int segments;

	BezierCurve(std::vector<sf::Vector2f> points, sf::Color col, int segs = 100)
		: controlPoints(points), segments(segs) {
		color = col;
	}

	void draw(std::vector<sf::RectangleShape>& pixels) override {
		if (controlPoints.size() < 2) return;

		// Draw control points
		for (auto& cp : controlPoints) {
			sf::RectangleShape marker(sf::Vector2f(6, 6));
			marker.setPosition(cp.x - 3, cp.y - 3);
			marker.setFillColor(sf::Color(100, 100, 100));
			pixels.push_back(marker);
		}

		// Draw curve using De Casteljau's algorithm
		sf::Vector2f prevPoint = evaluateBezier(0.0f);
		for (int i = 1; i <= segments; i++) {
			float t = static_cast<float>(i) / segments;
			sf::Vector2f currentPoint = evaluateBezier(t);

			drawBresenhamLine(pixels, prevPoint, currentPoint);
			prevPoint = currentPoint;
		}
	}

	bool containsPoint(sf::Vector2f point) override {
		// Check proximity to any control point
		for (auto& cp : controlPoints) {
			float dx = point.x - cp.x;
			float dy = point.y - cp.y;
			if (sqrt(dx * dx + dy * dy) < SELECTION_THRESHOLD) {
				return true;
			}
		}
		return false;
	}

	void translate(float dx, float dy) override {
		for (auto& cp : controlPoints) {
			cp.x += dx;
			cp.y += dy;
		}
	}

	void rotate(float angleDegrees) override {
		sf::Vector2f center = getCenter();
		float angleRad = angleDegrees * PI / 180.0f;

		for (auto& cp : controlPoints) {
			float dx = cp.x - center.x;
			float dy = cp.y - center.y;
			float xNew = dx * cos(angleRad) - dy * sin(angleRad);
			float yNew = dx * sin(angleRad) + dy * cos(angleRad);
			cp.x = center.x + xNew;
			cp.y = center.y + yNew;
		}
	}

	void scale(float factor) override {
		sf::Vector2f center = getCenter();
		for (auto& cp : controlPoints) {
			cp = center + (cp - center) * factor;
		}
	}

	sf::Vector2f getCenter() override {
		sf::Vector2f sum(0, 0);
		for (auto& cp : controlPoints) {
			sum += cp;
		}
		return sum / static_cast<float>(controlPoints.size());
	}

	std::string getInfo() override {
		std::stringstream ss;
		ss << "Bezier Curve | Control Points: " << controlPoints.size();
		return ss.str();
	}

private:
	sf::Vector2f evaluateBezier(float t) {
		// De Casteljau's algorithm for Bezier curve evaluation
		std::vector<sf::Vector2f> points = controlPoints;

		while (points.size() > 1) {
			std::vector<sf::Vector2f> newPoints;
			for (size_t i = 0; i < points.size() - 1; i++) {
				sf::Vector2f interpolated;
				interpolated.x = (1 - t) * points[i].x + t * points[i + 1].x;
				interpolated.y = (1 - t) * points[i].y + t * points[i + 1].y;
				newPoints.push_back(interpolated);
			}
			points = newPoints;
		}

		return points[0];
	}

	void drawBresenhamLine(std::vector<sf::RectangleShape>& pixels,
		sf::Vector2f p1, sf::Vector2f p2) {
		int x1 = static_cast<int>(std::round(p1.x));
		int y1 = static_cast<int>(std::round(p1.y));
		int x2 = static_cast<int>(std::round(p2.x));
		int y2 = static_cast<int>(std::round(p2.y));

		int dx = std::abs(x2 - x1);
		int dy = std::abs(y2 - y1);
		int sx = (x1 < x2) ? 1 : -1;
		int sy = (y1 < y2) ? 1 : -1;
		int err = dx - dy;

		while (true) {
			sf::RectangleShape pixel(sf::Vector2f(2, 2));
			pixel.setPosition(static_cast<float>(x1), static_cast<float>(y1));
			pixel.setFillColor(color);
			pixels.push_back(pixel);

			if (x1 == x2 && y1 == y2) break;

			int e2 = 2 * err;
			if (e2 > -dy) { err -= dy; x1 += sx; }
			if (e2 < dx) { err += dx; y1 += sy; }
		}
	}
};

// ============================================================================
// UI CLASS FOR DRAWING INTERFACE
// ============================================================================
class UI {
private:
	sf::Font font;
	bool fontLoaded;

public:
	UI() : fontLoaded(false) {
		// Try to load system font
		if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
			std::cout << "Warning: Could not load font. UI text disabled.\n";
		}
		else {
			fontLoaded = true;
		}
	}

	void drawHUD(sf::RenderWindow& window, const std::string& mode,
		const std::string& shapeInfo, int shapeCount) {
		if (!fontLoaded) return;

		// Mode indicator
		sf::Text modeText;
		modeText.setFont(font);
		modeText.setString("Mode: " + mode);
		modeText.setCharacterSize(18);
		modeText.setFillColor(sf::Color::White);
		modeText.setPosition(10, 10);
		window.draw(modeText);

		// Shape count
		sf::Text countText;
		countText.setFont(font);
		countText.setString("Shapes: " + std::to_string(shapeCount));
		countText.setCharacterSize(18);
		countText.setFillColor(sf::Color::White);
		countText.setPosition(10, 35);
		window.draw(countText);

		// Shape info
		if (!shapeInfo.empty()) {
			sf::Text infoText;
			infoText.setFont(font);
			infoText.setString(shapeInfo);
			infoText.setCharacterSize(16);
			infoText.setFillColor(sf::Color::Yellow);
			infoText.setPosition(10, 60);
			window.draw(infoText);
		}

		// Help text
		drawHelp(window);
	}

	void drawHelp(sf::RenderWindow& window) {
		if (!fontLoaded) return;

		std::vector<std::string> helpLines = {
			"MODES: 1=Select 2=DDA 3=Bresenham 4=Circle 5=Ellipse 6=Polygon 7=Bezier",
			"TRANSFORM: Arrows=Move Q/E=Rotate W/S=Scale",
			"OTHER: F=Fill Toggle | Del=Delete | C=Clear | ESC=Exit"
		};

		float yPos = WINDOW_HEIGHT - 80;
		for (auto& line : helpLines) {
			sf::Text helpText;
			helpText.setFont(font);
			helpText.setString(line);
			helpText.setCharacterSize(14);
			helpText.setFillColor(sf::Color(150, 150, 150));
			helpText.setPosition(10, yPos);
			window.draw(helpText);
			yPos += 20;
		}
	}

	void drawGrid(sf::RenderWindow& window, int gridSize = 50) {
		for (int x = 0; x < WINDOW_WIDTH; x += gridSize) {
			sf::Vertex line[] = {
				sf::Vertex(sf::Vector2f(static_cast<float>(x), 0), sf::Color(50, 50, 50)),
				sf::Vertex(sf::Vector2f(static_cast<float>(x), WINDOW_HEIGHT), sf::Color(50, 50, 50))
			};
			window.draw(line, 2, sf::Lines);
		}

		for (int y = 0; y < WINDOW_HEIGHT; y += gridSize) {
			sf::Vertex line[] = {
				sf::Vertex(sf::Vector2f(0, static_cast<float>(y)), sf::Color(50, 50, 50)),
				sf::Vertex(sf::Vector2f(WINDOW_WIDTH, static_cast<float>(y)), sf::Color(50, 50, 50))
			};
			window.draw(line, 2, sf::Lines);
		}
	}
};

// ============================================================================
// MAIN APPLICATION
// ============================================================================
int main() {
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
		"Advanced Mini-CAD: Multi-Algorithm Graphics Editor");
	window.setFramerateLimit(60);

	std::vector<std::shared_ptr<Shape>> shapes;
	std::vector<sf::Vector2f> tempPoints;

	enum Mode {
		SELECTION = 1,
		DRAW_DDA = 2,
		DRAW_BRESENHAM = 3,
		DRAW_CIRCLE = 4,
		DRAW_ELLIPSE = 5,
		DRAW_POLYGON = 6,
		DRAW_BEZIER = 7
	};

	Mode currentMode = SELECTION;
	std::shared_ptr<Shape> selectedShape = nullptr;
	bool fillPolygon = false;
	bool showGrid = true;

	UI ui;

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				window.close();
			}

			// Keyboard shortcuts
			if (event.type == sf::Event::KeyPressed) {
				switch (event.key.code) {
				case sf::Keyboard::Num1: currentMode = SELECTION; break;
				case sf::Keyboard::Num2: currentMode = DRAW_DDA; break;
				case sf::Keyboard::Num3: currentMode = DRAW_BRESENHAM; break;
				case sf::Keyboard::Num4: currentMode = DRAW_CIRCLE; break;
				case sf::Keyboard::Num5: currentMode = DRAW_ELLIPSE; break;
				case sf::Keyboard::Num6: currentMode = DRAW_POLYGON; break;
				case sf::Keyboard::Num7: currentMode = DRAW_BEZIER; break;
				case sf::Keyboard::F: fillPolygon = !fillPolygon; break;
				case sf::Keyboard::G: showGrid = !showGrid; break;
				case sf::Keyboard::C:
					shapes.clear();
					selectedShape = nullptr;
					tempPoints.clear();
					break;
				case sf::Keyboard::Escape:
					tempPoints.clear();
					break;
				case sf::Keyboard::Delete:
					if (selectedShape) {
						shapes.erase(std::remove(shapes.begin(), shapes.end(), selectedShape),
							shapes.end());
						selectedShape = nullptr;
					}
					break;
				}
			}

			// Mouse input
			if (event.type == sf::Event::MouseButtonPressed &&
				event.mouseButton.button == sf::Mouse::Left) {
				sf::Vector2f mousePos = window.mapPixelToCoords(
					sf::Mouse::getPosition(window));

				if (currentMode == SELECTION) {
					// Deselect all
					for (auto& shape : shapes) {
						shape->isSelected = false;
					}
					selectedShape = nullptr;

					// Find clicked shape (reverse order for top-most)
					for (auto it = shapes.rbegin(); it != shapes.rend(); ++it) {
						if ((*it)->containsPoint(mousePos)) {
							(*it)->isSelected = true;
							selectedShape = *it;
							break;
						}
					}
				}
				else if (currentMode == DRAW_DDA) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						shapes.push_back(std::make_shared<Line>(
							tempPoints[0], tempPoints[1], Line::DDA, sf::Color::Green));
						tempPoints.clear();
					}
				}
				else if (currentMode == DRAW_BRESENHAM) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						shapes.push_back(std::make_shared<Line>(
							tempPoints[0], tempPoints[1], Line::BRESENHAM, sf::Color::Red));
						tempPoints.clear();
					}
				}
				else if (currentMode == DRAW_CIRCLE) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						float dx = tempPoints[1].x - tempPoints[0].x;
						float dy = tempPoints[1].y - tempPoints[0].y;
						float radius = sqrt(dx * dx + dy * dy);
						shapes.push_back(std::make_shared<Circle>(
							tempPoints[0], radius, sf::Color::Blue));
						tempPoints.clear();
					}
				}
				else if (currentMode == DRAW_ELLIPSE) {
					tempPoints.push_back(mousePos);
					if (tempPoints.size() == 2) {
						float rx = std::abs(tempPoints[1].x - tempPoints[0].x);
						float ry = std::abs(tempPoints[1].y - tempPoints[0].y);
						shapes.push_back(std::make_shared<Ellipse>(
							tempPoints[0], rx, ry, sf::Color::Magenta));
						tempPoints.clear();
					}
				}
				else if (currentMode == DRAW_POLYGON) {
					tempPoints.push_back(mousePos);
				}
				else if (currentMode == DRAW_BEZIER) {
					tempPoints.push_back(mousePos);
				}
			}

			// Right click to finish polygon/bezier
			if (event.type == sf::Event::MouseButtonPressed &&
				event.mouseButton.button == sf::Mouse::Right) {
				if (currentMode == DRAW_POLYGON && tempPoints.size() >= 3) {
					shapes.push_back(std::make_shared<Polygon>(
						tempPoints, sf::Color::Cyan, fillPolygon));
					tempPoints.clear();
				}
				else if (currentMode == DRAW_BEZIER && tempPoints.size() >= 2) {
					shapes.push_back(std::make_shared<BezierCurve>(
						tempPoints, sf::Color::Yellow));
					tempPoints.clear();
				}
			}
		}

		// Real-time transformations
		if (selectedShape) {
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
				selectedShape->translate(-MOVE_AMOUNT, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
				selectedShape->translate(MOVE_AMOUNT, 0);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
				selectedShape->translate(0, -MOVE_AMOUNT);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
				selectedShape->translate(0, MOVE_AMOUNT);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q))
				selectedShape->rotate(-ROTATE_AMOUNT);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::E))
				selectedShape->rotate(ROTATE_AMOUNT);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::W))
				selectedShape->scale(SCALE_FACTOR_UP);
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
				selectedShape->scale(SCALE_FACTOR_DOWN);
		}

		// Render
		window.clear(sf::Color(25, 25, 35));

		if (showGrid) {
			ui.drawGrid(window);
		}

		std::vector<sf::RectangleShape> pixels;

		// Draw all shapes
		for (auto& shape : shapes) {
			shape->draw(pixels);
		}

		// Draw temp points for polygon/bezier
		for (auto& tp : tempPoints) {
			sf::CircleShape marker(5);
			marker.setPosition(tp.x - 5, tp.y - 5);
			marker.setFillColor(sf::Color::White);
			window.draw(marker);
		}

		// Draw all pixels
		for (auto& pixel : pixels) {
			window.draw(pixel);
		}

		// Highlight selected shape
		if (selectedShape) {
			sf::CircleShape highlight(8);
			sf::Vector2f center = selectedShape->getCenter();
			highlight.setPosition(center.x - 8, center.y - 8);
			highlight.setFillColor(sf::Color::Transparent);
			highlight.setOutlineColor(sf::Color::Yellow);
			highlight.setOutlineThickness(2);
			window.draw(highlight);
		}

		// Draw UI
		std::string modeStr;
		switch (currentMode) {
		case SELECTION: modeStr = "Selection"; break;
		case DRAW_DDA: modeStr = "DDA Line"; break;
		case DRAW_BRESENHAM: modeStr = "Bresenham Line"; break;
		case DRAW_CIRCLE: modeStr = "Circle"; break;
		case DRAW_ELLIPSE: modeStr = "Ellipse"; break;
		case DRAW_POLYGON: modeStr = "Polygon"; break;
		case DRAW_BEZIER: modeStr = "Bezier Curve"; break;
		}

		std::string shapeInfo = selectedShape ? selectedShape->getInfo() : "";
		ui.drawHUD(window, modeStr, shapeInfo, static_cast<int>(shapes.size()));

		window.display();
	}

	return 0;
}