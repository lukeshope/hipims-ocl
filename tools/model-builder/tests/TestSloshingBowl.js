'use strict';

const TestCaseBase = require('../TestCaseBase');
const Gravity = 9.806;

function TestSloshingBowl () {
	TestCaseBase.apply(this, Array.prototype.slice.call(arguments));
	
	this.bowlH0 = this.parentDomain.parentModel.getConstant('h0') || 10.0;		// Scaling factor
	this.bowlAlpha = this.parentDomain.parentModel.getConstant('a') || 3000.0;	// Sloping factor
	this.bowlBeta = this.parentDomain.parentModel.getConstant('B') || 5.0;		// Initial velocity (m/s)
	this.bowlTau = this.parentDomain.parentModel.getConstant('t') || 0.0;		// Friction parameter
	
	this.calculateDerivatives();
};
TestSloshingBowl.prototype = new TestCaseBase();

TestSloshingBowl.prototype.calculateDerivatives = function () {
	this.bowlPeakAmplitude = Math.sqrt(8 * Gravity * this.bowlH0 / Math.pow(this.bowlAlpha, 2));
	this.bowlS = Math.sqrt(Math.pow(this.bowlPeakAmplitude, 2) - Math.pow(this.bowlTau, 2)) / 2.0;
}

TestSloshingBowl.prototype.getDescription = function () {
	return '    2D sloshing parabolic bowl, with or without ' +
		 '\n    friction. Normally requires MUSCL-Hancock ' +
		 '\n    scheme to prevent diffusion affecting the' +
		 '\n    results severely over time.' +
		 '\n' +
		 '\n    Wang et al. (2011) A 2D shallow flow model' +
		 '\n    for practical dam-break simulations, Journal' +
		 '\n    of Hydraulic Research, 49(3):307-316.';
}

TestCaseBase.prototype.getManningCoefficient = function () {
	return 0.0;
}

TestSloshingBowl.prototype.getTopography = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueBed
	);
}

TestSloshingBowl.prototype.getInitialFSL = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueFSL
	);
}

TestSloshingBowl.prototype.getInitialVelocityX = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueVelocityX
	);
}

TestSloshingBowl.prototype.getInitialVelocityY = function (domainSizeX, domainSizeY, domainResolution) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueVelocityY
	);
}

TestSloshingBowl.prototype.getDepthAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return this.getGridUsingFormula(
		domainSizeX, 
		domainSizeY,
		domainResolution,
		this.getValueDepth,
		simulationTime
	);
}

TestSloshingBowl.prototype.getValueBed = function (x, y) {
	return this.bowlH0 * (Math.pow(x, 2) + Math.pow(y, 2)) / Math.pow(this.bowlAlpha, 2);
}

TestSloshingBowl.prototype.getValueDepth = function (x, y, t) {
	return Math.round((this.getValueFSL(x, y, t) - this.getValueBed(x, y)) * 100) / 100;
}

TestSloshingBowl.prototype.getValueFSL = function (x, y, t) {
	if (t === undefined) t = 0.0;
	let bed = this.getValueBed(x, y);
	let fsl = this.bowlH0 
			  - (1.0/Gravity) * this.bowlBeta * Math.pow(Math.E, -1 * this.bowlTau * t * 0.5) * ((this.bowlTau / 2.0) * Math.sin(this.bowlS * t) + this.bowlS * Math.cos(this.bowlS * t)) * x
			  - (1.0/Gravity) * this.bowlBeta * Math.pow(Math.E, -1 * this.bowlTau * t * 0.5) * ((this.bowlTau / 2.0) * Math.cos(this.bowlS * t) - this.bowlS * Math.sin(this.bowlS * t)) * y;
	return fsl > this.getValueBed(x, y) ? fsl : bed;
}

TestSloshingBowl.prototype.getValueVelocityX = function (x, y, t) {
	if (t === undefined) t = 0.0;
	return this.bowlBeta * Math.pow(Math.E, -this.bowlTau * t * 0.5) * Math.sin(this.bowlS * t);
}

TestSloshingBowl.prototype.getValueVelocityY = function (x, y, t) {
	if (t === undefined) t = 0.0;
	return -1 * this.bowlBeta * Math.pow(Math.E, -this.bowlTau * t * 0.5) * Math.cos(this.bowlS * t);
}

module.exports = TestSloshingBowl;
