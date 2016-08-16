'use strict';

function Point (x, y) {
	this.x = x;
	this.y = y;
};

Point.prototype.isLeftOf = function (lineStart, lineEnd) {
	return ( (lineEnd.x - lineStart.x) * (this.y - lineStart.y) - 
	         (this.x - lineStart.x) * (lineEnd.y - lineStart.y) );
}

module.exports = Point;
