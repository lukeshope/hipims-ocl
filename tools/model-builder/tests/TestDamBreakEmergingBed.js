'use strict';

const TestCaseBase = require('../TestCaseBase');
const Gravity = 9.806;

function TestDamBreakEmergingBed () {
	TestCaseBase.apply(this, Array.prototype.slice.call(arguments));
	
	this.h0 = 1.0;
	this.wallHeight = this.parentDomain.parentModel.getConstant('w') || 2.0;			// Wall height
	this.slopeAngle = this.parentDomain.parentModel.getConstant('a') || Math.PI / 60.0; // Bed slope
	this.damLevel = this.parentDomain.parentModel.getConstant('n') || 1.0; 				// Depth behind dam
	this.damPosition = this.parentDomain.parentModel.getConstant('p') || 0.0; 			// Dam position along X-axis
};
TestDamBreakEmergingBed.prototype = new TestCaseBase();

TestDamBreakEmergingBed.prototype.getDescription = function () {
	return '    2D domain with a dam break over an emerging ' +
		 '\n    bed, for which the front location is known. ' +
		 '\n' +
		 '\n    Details from Xing et al. (2010) Positivity-' +
		 '\n    preserving high order well-balanced discontinuous' +
		 '\n    Galerkin methods for the shallow water equations, ' +
		 '\n    Advances in Water Resources, 33:1476-1493.';
}

TestDamBreakEmergingBed.prototype.getManningCoefficient = function () {
	return 0.0;
}

TestDamBreakEmergingBed.prototype.getTopography = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueBed
	);
}

TestDamBreakEmergingBed.prototype.getInitialDepth = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueDepth
	);
}

TestDamBreakEmergingBed.prototype.getFrontPositionAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueFrontCode,
		simulationTime
	);
}

TestDamBreakEmergingBed.prototype.getFrontVelocityAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueFrontVelocity,
		simulationTime
	);
}

TestDamBreakEmergingBed.prototype.getValueFrontCode = function (x, y, t, domainMetadata) {
	let frontX = 2 * t * Math.sqrt(Gravity * this.h0 * Math.cos(this.slopeAngle)) - (0.5 * Gravity * Math.pow(t, 2.0) * Math.tan(this.slopeAngle));
	let bed = this.getValueBed(x, y, t, domainMetadata);
	let numericalResolution = domainMetadata.resolution;
	frontX = Math.floor((frontX - numericalResolution / 2) / numericalResolution) * numericalResolution + numericalResolution / 2;
	if (bed >= this.wallHeight) {
		return 0;
	}
	return (x <= frontX + numericalResolution * 0.75) ? (Math.abs(x - frontX) <= numericalResolution / 2 ? 2 : 1) : 0;
}

TestDamBreakEmergingBed.prototype.getValueFrontVelocity = function (x, y, t, domainMetadata) {
	let frontPosition = this.getValueFrontCode(x, y, t, domainMetadata);
	if (frontPosition !== 2) return null;
	let frontVelocity = 2 * Math.sqrt(Gravity * this.h0 * Math.cos(this.slopeAngle)) - (Gravity * Math.pow(t, 2.0) * Math.tan(this.slopeAngle));
	return frontVelocity;
}

TestDamBreakEmergingBed.prototype.getValueBed = function (x, y, t, domainMetadata) {
	if (Math.abs(x - domainMetadata.minX) <= domainMetadata.resolution * 1.1 ||
	    Math.abs(x - domainMetadata.maxX) <= domainMetadata.resolution * 1.1 ||
		Math.abs(y - domainMetadata.minY) <= domainMetadata.resolution * 1.1 ||
		Math.abs(y - domainMetadata.maxY) <= domainMetadata.resolution * 1.1) {
		return this.wallHeight;
	}
	return x * Math.tan(this.slopeAngle);
}

TestDamBreakEmergingBed.prototype.getValueDepth = function (x, y, t, domainMetadata) {
	let bed = this.getValueBed(x, y, t, domainMetadata);
	return x <= this.damPosition && this.damLevel > bed ? this.damLevel - bed : 0.0;
}

TestDamBreakEmergingBed.prototype.getValueFSL = function (x, y, t, domainMetadata) {
	return this.getValueBed(x, y, t, domainMetadata) + this.getValueDepth(x, y, t, domainMetadata);
}

module.exports = TestDamBreakEmergingBed;
