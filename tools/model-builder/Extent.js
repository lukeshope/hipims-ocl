'use strict';

var bngConversion = require('./BngConversion');

function Extent (x1, y1, x2, y2) {
	this.lowerLeftX = Math.min(x1, x2);
	this.lowerLeftY = Math.min(y1, y2);
	this.upperRightX = Math.max(x1, x2);
	this.upperRightY = Math.max(y1, y2);
};

Extent.prototype.snapToGrid = function (resolution) {
	this.lowerLeftX = Math.floor(this.lowerLeftX / resolution) * resolution;
	this.lowerLeftY = Math.floor(this.lowerLeftY / resolution) * resolution;
	this.upperRightX = Math.ceil(this.upperRightX / resolution) * resolution;
	this.upperRightY = Math.ceil(this.upperRightY / resolution) * resolution;
}

Extent.prototype.getSizeX = function (resolution) {
	return Math.round((this.upperRightX - this.lowerLeftX) / resolution); 
}

Extent.prototype.getSizeY = function (resolution) {
	return Math.round((this.upperRightY - this.lowerLeftY) / resolution); 
}

Extent.prototype.getLowerX = function () {
	return this.lowerLeftX;
}

Extent.prototype.getLowerY = function () {
	return this.lowerLeftY;
}

Extent.prototype.getUpperX = function () {
	return this.upperRightX;
}

Extent.prototype.getUpperY = function () {
	return this.upperRightY;
}

Extent.prototype.getBngTileNames = function() {
	const lidarTileGroupSize = 10000;
	let lidarTileSet = [];
	for (let e  = Math.floor(this.lowerLeftX / lidarTileGroupSize) * lidarTileGroupSize; 
	         e  < Math.ceil(this.upperRightX / lidarTileGroupSize) * lidarTileGroupSize; 
			 e += lidarTileGroupSize) {
		for (let n  = Math.floor(this.lowerLeftY / lidarTileGroupSize) * lidarTileGroupSize; 
	             n  < Math.ceil(this.upperRightY / lidarTileGroupSize) * lidarTileGroupSize; 
			     n += lidarTileGroupSize) {
			lidarTileSet.push(bngConversion.enToRef(e, n, 1));
		}
	}

	return lidarTileSet;
}

module.exports = Extent;
