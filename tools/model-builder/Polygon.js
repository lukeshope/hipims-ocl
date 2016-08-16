'use strict';

const Point = require('./Point');

function Polygon (points) {
	this.points = points;
		
	// Ensure it's closed
	const lastPoint = this.points[this.points.length - 1];
	const firstPoint = this.points[0];
	if (firstPoint !== lastPoint &&
	    (firstPoint && (firstPoint.x !== lastPoint.x || firstPoint.y !== lastPoint.y))) {
		this.points.push(firstPoint);
	}
};

Polygon.prototype.containsPoint = function (testPoint) {
	let wn = 0;
	let v = this.points;
	
	for (let i = 0; i < v.length - 1; i++) {
		if (v[i].y <= testPoint.y) {
			if (v[i + 1].y > testPoint.y) {
				if (testPoint.isLeftOf(v[i], v[i + 1]) > 0) {
					wn++;
				}
			}
		} else {
			if (v[i + 1].y <= testPoint.y) {
				if (testPoint.isLeftOf(v[i], v[i + 1]) < 0) {
					wn--;
				}
			}
		}
	}

	return wn !== 0 ? true : false;
}

module.exports = Polygon;
