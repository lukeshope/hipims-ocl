'use strict';

var bngConversion = require('./bngConversion');

var extent = function(x1, y1, x2, y2) {
	this.lowerLeftX = Math.min(x1, x2);
	this.lowerLeftY = Math.min(y1, y2);
	this.upperRightX = Math.max(x1, x2);
	this.upperRightY = Math.max(y1, y2);
};

extent.prototype.getBngTileNames = function() {
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

module.exports = extent;
