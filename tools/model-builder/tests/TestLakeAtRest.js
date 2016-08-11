'use strict';

const TestCaseBase = require('../TestCaseBase');

function TestLakeAtRest () {
	TestCaseBase.apply(this, Array.prototype.slice.call(arguments));
	
	this.shapeFactor = this.parentDomain.parentModel.getConstant('a') || 2000.0;		// Shape factor
	this.scaleFactor = this.parentDomain.parentModel.getConstant('b') || 5000.0;		// Scaling factor
	this.waterLevel = this.parentDomain.parentModel.getConstant('n') || 0.0;			// Water level
	this.islandLevel = this.parentDomain.parentModel.getConstant('i') || 100.0;			// Island level
	this.seaDepth = this.parentDomain.parentModel.getConstant('s') || 50.0;				// Max sea depth
};
TestLakeAtRest.prototype = new TestCaseBase();

TestLakeAtRest.prototype.getDescription = function () {
	return '    2D domain with a smooth island in the centre ' +
		 '\n    and no friction. No change in water level ' +
		 '\n    should occur.' +
		 '\n' +
		 '\n    Adapted from Xing et al. (2010) Positivity-' +
		 '\n    preserving high order well-balanced discontinuous' +
		 '\n    Galerkin methods for the shallow water equations, ' +
		 '\n    Advances in Water Resources, 33:1476-1493.';
}

TestLakeAtRest.prototype.getManningCoefficient = function () {
	return 0.0;
}

TestLakeAtRest.prototype.getTopography = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueBed
	);
}

TestLakeAtRest.prototype.getInitialFSL = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueFSL
	);
}

TestLakeAtRest.prototype.getDepthAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueDepth,
		simulationTime
	);
}

TestLakeAtRest.prototype.getValueBed = function (x, y) {
	return Math.max(this.islandLevel - (this.scaleFactor * (Math.pow(x, 2) + Math.pow(y, 2)) / Math.pow(this.shapeFactor, 2)), this.waterLevel - this.seaDepth);
}

TestLakeAtRest.prototype.getValueDepth = function (x, y, t) {
	return Math.round((this.getValueFSL(x, y, t) - this.getValueBed(x, y)) * 100) / 100;
}

TestLakeAtRest.prototype.getValueFSL = function (x, y, t) {
	let bed = this.getValueBed(x, y);
	return this.waterLevel > this.getValueBed(x, y) ? this.waterLevel : bed;
}

module.exports = TestLakeAtRest;
