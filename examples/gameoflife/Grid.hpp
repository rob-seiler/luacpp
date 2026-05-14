#ifndef LUACPP_EXAMPLES_GOL_GRID_HPP
#define LUACPP_EXAMPLES_GOL_GRID_HPP

#include <algorithm>
#include <string>
#include <vector>

class Grid {
public:
	int width;
	int height;
	int generation;

	Grid(int w, int h)
		: width(w), height(h), generation(0), cells(static_cast<size_t>(w * h), false) {}

	void set(int x, int y, bool alive) {
		if (inBounds(x, y)) cells[idx(x, y)] = alive;
	}

	bool get(int x, int y) const {
		return inBounds(x, y) ? cells[idx(x, y)] : false;
	}

	void clear() {
		std::fill(cells.begin(), cells.end(), false);
		generation = 0;
	}

	int countAlive() const {
		int c = 0;
		for (bool b : cells) if (b) ++c;
		return c;
	}

	void step() {
		std::vector<bool> next(cells.size(), false);
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				int n = neighbors(x, y);
				bool alive = cells[idx(x, y)];
				next[idx(x, y)] = alive ? (n == 2 || n == 3) : (n == 3);
			}
		}
		cells = std::move(next);
		++generation;
	}

	bool operator==(const Grid& other) const {
		return width == other.width && height == other.height && cells == other.cells;
	}

	std::string toString() const {
		std::string s;
		s.reserve(static_cast<size_t>((width + 1) * height));
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				s += cells[idx(x, y)] ? '#' : '.';
			}
			s += '\n';
		}
		return s;
	}

private:
	std::vector<bool> cells;

	int idx(int x, int y) const { return y * width + x; }
	bool inBounds(int x, int y) const { return x >= 0 && x < width && y >= 0 && y < height; }

	int neighbors(int x, int y) const {
		int c = 0;
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				if (dx == 0 && dy == 0) continue;
				if (get(x + dx, y + dy)) ++c;
			}
		}
		return c;
	}
};

#endif // LUACPP_EXAMPLES_GOL_GRID_HPP
