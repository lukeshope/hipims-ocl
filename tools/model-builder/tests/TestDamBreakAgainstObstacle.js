'use strict';

const TestCaseBase = require('../TestCaseBase');
const Extent = require('../Extent');
const Polygon = require('../Polygon');
const Point = require('../Point');
const Gravity = 9.806;

function TestDamBreakAgainstObstacle () {
	TestCaseBase.apply(this, Array.prototype.slice.call(arguments));
	
	this.domainManningCoefficient = 0.01;
	this.domainWallWidth = 0.2;
	this.domainWallExtrusion = 0.5;
	this.domainFlumeWidth = 3.6;
	this.domainFlumeLength = 35.8;
	this.domainEdgeHeight = 0.155;
	this.domainEdgeLength = 0.34;
	this.obstacleExtrusion = 0.5;
	this.obstacleLength = 0.8;
	this.obstacleWidth = 0.4;
	this.obstacleRotation = 64 / 180 * Math.PI;
	this.obstacleOffsetX = 10.99;
	this.obstacleOffsetY = 1.75;
	this.gateOffsetX = 6.75;
	this.gateWidth = 0.8;
	this.gateOpeningSize = 1.0;
	this.gateExtrusion = 0.5;
	this.damDepth = 0.4;
	this.flumeDepth = 0.02;
	
	this.buildObstacles();
};
TestDamBreakAgainstObstacle.prototype = new TestCaseBase();

TestDamBreakAgainstObstacle.prototype.getDescription = function () {
	return '    2D domain with a dam break against an obstacle ' +
		 '\n    for comparison against laboratory results. ' +
		 '\n' +
		 '\n    Details from Soares-Frazão, S. and Zech, Y. (2007)' +
		 '\n    Experimental study of dam-break flow against an' +
		 '\n    isolated obstacle, Journal of Hydraulic Research,' +
		 '\n    45:sup1:27-36.';
}

TestDamBreakAgainstObstacle.prototype.buildObstacles = function () {
	const obstacleOrigin = new Point(this.obstacleOffsetX, this.obstacleOffsetY);
	const obstacle = new Polygon([
		obstacleOrigin,
		new Point(
			obstacleOrigin.x + Math.cos(this.obstacleRotation) * this.obstacleLength, 
			obstacleOrigin.y + Math.sin(this.obstacleRotation) * this.obstacleLength
		),
		new Point(
			obstacleOrigin.x + Math.cos(this.obstacleRotation) * this.obstacleLength + Math.cos(Math.PI / 2 - this.obstacleRotation) * this.obstacleWidth, 
			obstacleOrigin.y + Math.sin(this.obstacleRotation) * this.obstacleLength - Math.sin(Math.PI / 2 - this.obstacleRotation) * this.obstacleWidth
		),
		new Point(
			obstacleOrigin.x + Math.cos(Math.PI / 2 - this.obstacleRotation) * this.obstacleWidth, 
			obstacleOrigin.y - Math.sin(Math.PI / 2 - this.obstacleRotation) * this.obstacleWidth
		)
	]);
	
	const gateLeft = new Polygon([
		new Point(this.gateOffsetX, 0.0),
		new Point(this.gateOffsetX, (this.domainFlumeWidth - this.gateOpeningSize) / 2),
		new Point(this.gateOffsetX + this.gateWidth, (this.domainFlumeWidth - this.gateOpeningSize) / 2),
		new Point(this.gateOffsetX + this.gateWidth, 0.0)
	]);
	
	const gateRight = new Polygon([
		new Point(this.gateOffsetX, this.domainFlumeWidth),
		new Point(this.gateOffsetX, this.domainFlumeWidth / 2 + this.gateOpeningSize / 2),
		new Point(this.gateOffsetX + this.gateWidth, this.domainFlumeWidth / 2 + this.gateOpeningSize / 2),
		new Point(this.gateOffsetX + this.gateWidth, this.domainFlumeWidth)
	]);
	
	this._obstacle = obstacle;
	this._gateRight = gateLeft;
	this._gateLeft = gateRight;
}

TestDamBreakAgainstObstacle.prototype.getExtent = function () {
	return new Extent(0.0, 0.0, this.domainFlumeLength + 2 * this.domainWallWidth, this.domainFlumeWidth + 2 * this.domainWallWidth);
}

TestDamBreakAgainstObstacle.prototype.getManningCoefficient = function () {
	return this.domainManningCoefficient;
}

TestDamBreakAgainstObstacle.prototype.getRealativePoint = function (x, y, domainMetadata) {
	return new Point(x - domainMetadata.minX - this.domainWallWidth + domainMetadata.resolution / 2, domainMetadata.maxY - y - this.domainWallWidth + domainMetadata.resolution / 2);
}

TestDamBreakAgainstObstacle.prototype.getTopography = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueBed
	);
}

TestDamBreakAgainstObstacle.prototype.getInitialDepth = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueDepth
	);
}

TestDamBreakAgainstObstacle.prototype.getValueBed = function (x, y, t, domainMetadata) {
	const domainPoint = this.getRealativePoint(x, y, domainMetadata);
	
	if (domainPoint.x < 0.0 || domainPoint.y < 0.0 || domainPoint.x > this.domainFlumeLength || domainPoint.y > this.domainFlumeWidth) {
		return this.domainWallExtrusion;
	}
	
	if (this._obstacle.containsPoint(domainPoint)) {
		return this.obstacleExtrusion;
	}
	
	if (this._gateLeft.containsPoint(domainPoint) || this._gateRight.containsPoint(domainPoint)) {
		return this.gateExtrusion;
	}
	
	if (domainPoint.y >= this.domainFlumeWidth - this.domainEdgeLength) {
		return Math.max(this.domainEdgeHeight - (this.domainFlumeWidth - domainPoint.y) * this.domainEdgeHeight / this.domainEdgeLength, 0.0);
	}
	
	if (domainPoint.y <= this.domainEdgeLength) {
		return Math.max(this.domainEdgeHeight - domainPoint.y * this.domainEdgeHeight / this.domainEdgeLength, 0.0);
	}
	
	return 0.0;
}

TestDamBreakAgainstObstacle.prototype.getValueDepth = function (x, y, t, domainMetadata) {
	const domainPoint = this.getRealativePoint(x, y, domainMetadata);
	const bedLevel = this.getValueBed(x, y, t, domainMetadata)
	
	if (domainPoint.x < 0.0 || domainPoint.y < 0.0 || domainPoint.x > this.domainFlumeLength || domainPoint.y > this.domainFlumeWidth) {
		return 0.0;
	}
	
	if (bedLevel >= this.domainWallExtrusion) {
		return 0.0;
	}
	
	if (domainPoint.x < this.gateOffsetX + this.gateWidth) {
		return Math.max(this.damDepth - bedLevel, 0.0);
	}
	
	return Math.max(this.flumeDepth - bedLevel, 0.0);
}

TestDamBreakAgainstObstacle.prototype.getValueFSL = function (x, y, t, domainMetadata) {
	return this.getValueBed(x, y, t, domainMetadata) + this.getValueDepth(x, y, t, domainMetadata);
}

module.exports = TestDamBreakAgainstObstacle;
