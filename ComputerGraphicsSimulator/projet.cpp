#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>

// Custom math functions (no library math for core physics)
class Physics {
public:
	// Manual square root using Newton's method
	static float customSqrt(float x) {
		if (x < 0) return 0;
		if (x == 0) return 0;

		float guess = x / 2.0f;
		float epsilon = 0.00001f;

		for (int i = 0; i < 20; i++) {
			float newGuess = (guess + x / guess) / 2.0f;
			if (customAbs(newGuess - guess) < epsilon) break;
			guess = newGuess;
		}
		return guess;
	}

	static float customAbs(float x) {
		return x < 0 ? -x : x;
	}

	static float customPow(float base, int exp) {
		float result = 1.0f;
		for (int i = 0; i < exp; i++) {
			result *= base;
		}
		return result;
	}
};

class Ball {
public:
	float x, y;           // Position
	float vx, vy;         // Velocity
	float radius;
	float mass;
	float angle;          // Rotation angle
	sf::Color color;

	Ball(float px, float py, float r, float m, sf::Color c)
		: x(px), y(py), vx(0), vy(0), radius(r), mass(m), angle(0), color(c) {
	}

	void draw(sf::RenderWindow& window) {
		sf::CircleShape circle(radius);
		circle.setPosition(x - radius, y - radius);
		circle.setFillColor(color);
		circle.setOutlineThickness(2);
		circle.setOutlineColor(sf::Color::Black);

		// Draw a line to show rotation
		sf::Vertex line[2];
		line[0].position = sf::Vector2f(x, y);
		line[0].color = sf::Color::Black;

		// Using custom approximation for cos/sin
		float cosAngle = approximateCos(angle);
		float sinAngle = approximateSin(angle);

		line[1].position = sf::Vector2f(x + radius * cosAngle, y + radius * sinAngle);
		line[1].color = sf::Color::Black;

		window.draw(circle);
		window.draw(line, 2, sf::Lines);
	}

private:
	// Taylor series approximation for cosine
	float approximateCos(float x) {
		// Normalize angle to [-PI, PI]
		const float PI = 3.14159265f;
		while (x > PI) x -= 2 * PI;
		while (x < -PI) x += 2 * PI;

		float x2 = x * x;
		float x4 = x2 * x2;
		float x6 = x4 * x2;
		float x8 = x6 * x2;

		return 1.0f - x2 / 2.0f + x4 / 24.0f - x6 / 720.0f + x8 / 40320.0f;
	}

	// Taylor series approximation for sine
	float approximateSin(float x) {
		const float PI = 3.14159265f;
		while (x > PI) x -= 2 * PI;
		while (x < -PI) x += 2 * PI;

		float x2 = x * x;
		float x3 = x2 * x;
		float x5 = x3 * x2;
		float x7 = x5 * x2;

		return x - x3 / 6.0f + x5 / 120.0f - x7 / 5040.0f;
	}
};

class Terrain {
public:
	std::vector<sf::Vector2f> points;

	void addPoint(float x, float y) {
		points.push_back(sf::Vector2f(x, y));
	}

	void draw(sf::RenderWindow& window) {
		if (points.size() < 2) return;

		// Draw terrain outline
		for (size_t i = 0; i < points.size() - 1; i++) {
			sf::Vertex line[] = {
				sf::Vertex(points[i], sf::Color::Green),
				sf::Vertex(points[i + 1], sf::Color::Green)
			};
			window.draw(line, 2, sf::Lines);
		}

		// Fill terrain
		sf::ConvexShape terrain;
		terrain.setPointCount(points.size() + 2);
		for (size_t i = 0; i < points.size(); i++) {
			terrain.setPoint(i, points[i]);
		}
		// Close the shape at bottom
		terrain.setPoint(points.size(), sf::Vector2f(800, 600));
		terrain.setPoint(points.size() + 1, sf::Vector2f(0, 600));
		terrain.setFillColor(sf::Color(34, 139, 34));

		window.draw(terrain);
	}

	// Find height at given x position using linear interpolation
	float getHeightAt(float x) {
		if (points.size() < 2) return 0;

		for (size_t i = 0; i < points.size() - 1; i++) {
			if (x >= points[i].x && x <= points[i + 1].x) {
				// Linear interpolation
				float t = (x - points[i].x) / (points[i + 1].x - points[i].x);
				return points[i].y + t * (points[i + 1].y - points[i].y);
			}
		}
		return points.back().y;
	}

	// Get slope angle at position x (in radians approximation)
	float getSlopeAt(float x) {
		if (points.size() < 2) return 0;

		for (size_t i = 0; i < points.size() - 1; i++) {
			if (x >= points[i].x && x <= points[i + 1].x) {
				float dx = points[i + 1].x - points[i].x;
				float dy = points[i + 1].y - points[i].y;
				// Return slope as angle approximation
				return dy / dx; // This gives us tangent, good enough for small angles
			}
		}
		return 0;
	}
};

int main() {
	sf::RenderWindow window(sf::VideoMode(800, 600), "Rolling Ball Physics Simulation");
	window.setFramerateLimit(60);

	// Create terrain with hills
	Terrain terrain;
	terrain.addPoint(0, 400);
	terrain.addPoint(150, 350);
	terrain.addPoint(250, 300);
	terrain.addPoint(350, 380);
	terrain.addPoint(450, 450);
	terrain.addPoint(550, 420);
	terrain.addPoint(650, 480);
	terrain.addPoint(800, 500);

	Ball* ball = nullptr;
	bool isDragging = false;
	float ballRadius = 20.0f;
	float ballMass = 1.0f;

	// Physics constants
	const float GRAVITY = 500.0f;  // pixels/s^2
	const float FRICTION = 0.3f;   // coefficient of rolling friction
	const float DAMPING = 0.98f;   // energy loss on collision
	const float dt = 1.0f / 60.0f; // time step

	sf::Font font;
	sf::Text instructions;

	// Create simple instructions
	instructions.setString("Click and drag to create a ball, then release to drop it!");
	instructions.setCharacterSize(16);
	instructions.setFillColor(sf::Color::White);
	instructions.setPosition(10, 10);

	sf::Clock clock;

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();

			if (event.type == sf::Event::MouseButtonPressed) {
				if (event.mouseButton.button == sf::Mouse::Left) {
					float mx = event.mouseButton.x;
					float my = event.mouseButton.y;

					// Create new ball
					delete ball;
					ball = new Ball(mx, my, ballRadius, ballMass, sf::Color::Red);
					isDragging = true;
				}
			}

			if (event.type == sf::Event::MouseButtonReleased) {
				isDragging = false;
			}

			if (event.type == sf::Event::MouseMoved && isDragging && ball) {
				ball->x = event.mouseMove.x;
				ball->y = event.mouseMove.y;
			}
		}

		// Physics update
		if (ball && !isDragging) {
			// Get terrain info at ball position
			float terrainHeight = terrain.getHeightAt(ball->x);
			float slope = terrain.getSlopeAt(ball->x);

			// Check if ball is on or below terrain
			if (ball->y + ball->radius >= terrainHeight) {
				// Ball is on terrain
				ball->y = terrainHeight - ball->radius;

				// Calculate gravity component along slope
				// For small angles: sin(theta) ≈ tan(theta) = slope
				float slopeAngle = slope;
				float normalizedSlope = slopeAngle / Physics::customSqrt(1 + slopeAngle * slopeAngle);

				// Acceleration along slope = g * sin(theta) - friction * g * cos(theta)
				// For rolling: a = (g * sin(theta)) / (1 + I/(m*r^2))
				// For solid sphere: I/(m*r^2) = 2/5, so divisor = 1.4
				float accelAlongSlope = (GRAVITY * normalizedSlope) / 1.4f;

				// Friction opposes motion
				float frictionForce = FRICTION * GRAVITY * 0.3f;
				if (ball->vx > 0.1f || ball->vx < -0.1f) {
					accelAlongSlope -= (ball->vx > 0 ? frictionForce : -frictionForce);
				}

				// Update velocity
				ball->vx += accelAlongSlope * dt;
				ball->vx *= 0.995f; // Air resistance

				// Update position
				ball->x += ball->vx * dt;

				// Update rotation based on rolling (v = r * omega)
				ball->angle += (ball->vx / ball->radius) * dt;

				// Keep vertical velocity minimal when on ground
				ball->vy = 0;

			}
			else {
				// Ball is in air - free fall
				ball->vy += GRAVITY * dt;
				ball->y += ball->vy * dt;
				ball->x += ball->vx * dt;

				// Continue rotation in air
				ball->angle += (ball->vx / ball->radius) * dt;
			}

			// Bounce off walls
			if (ball->x - ball->radius < 0) {
				ball->x = ball->radius;
				ball->vx *= -DAMPING;
			}
			if (ball->x + ball->radius > 800) {
				ball->x = 800 - ball->radius;
				ball->vx *= -DAMPING;
			}
		}

		// Render
		window.clear(sf::Color(135, 206, 235)); // Sky blue
		terrain.draw(window);
		if (ball) ball->draw(window);
		window.draw(instructions);
		window.display();
	}

	delete ball;
	return 0;
}