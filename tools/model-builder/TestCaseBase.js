'use strict';

function TestCaseBase (parentDomain) {
	this.parentDomain = parentDomain;
};

TestCaseBase.prototype.getGridUsingFormula = function (domainSizeX, domainSizeY, domainResolution, cellFormula, timeValue) {
	let domainData = new Float32Array(domainSizeX * domainSizeY);
	let domainShiftX = Math.round(domainSizeX / domainResolution) % 2 === 0 ? domainResolution / 2 : 0.0;
	let domainShiftY = Math.round(domainSizeY / domainResolution) % 2 === 0 ? domainResolution / 2 : 0.0;
	let domainMetadata = {
		minX: domainResolution * domainSizeX / -2 + domainShiftX,
		maxX: domainResolution * domainSizeX / 2 - domainShiftX,
		minY: domainResolution * domainSizeY / -2 + domainShiftY,
		maxY: domainResolution * domainSizeY / 2 - domainShiftY,
		resolution: domainResolution
	};
	
	// Reverse the direction of the Y-axis for these tests to match our LL origin
	for (let x = 0; x < domainSizeX; x++ ) {
		for (let y = 0; y < domainSizeY; y++) {
			let x0 = domainMetadata.minX + x * domainResolution;
			let y0 = domainMetadata.maxY - y * domainResolution;
			let a = y * domainSizeX + x;
			domainData[a] = cellFormula.call(this, x0, y0, timeValue, domainMetadata);
		}
	}
	
	return domainData;
}

TestCaseBase.prototype.getExtent = function () {
	return null;
}

TestCaseBase.prototype.getResolution = function () {
	return null;
}

TestCaseBase.prototype.getManningCoefficient = function () {
	return null;
}

TestCaseBase.prototype.getVaryingManningCoefficient = function () {
	return null;
}

TestCaseBase.prototype.getTopography = function (domainSizeX, domainSizeY, domainResolution) {
	return null;
}

TestCaseBase.prototype.getVaryingManningCoefficient = function (domainSizeX, domainSizeY, domainResolution) {
	return null;
}

TestCaseBase.prototype.getInitialDepth = function (domainSizeX, domainSizeY, domainResolution) {
	return null;
}

TestCaseBase.prototype.getInitialFSL = function (domainSizeX, domainSizeY, domainResolution) {
	return null;
}

TestCaseBase.prototype.getInitialVelocityX = function (domainSizeX, domainSizeY, domainResolution) {
	return null;
}

TestCaseBase.prototype.getInitialVelocityY = function (domainSizeX, domainSizeY, domainResolution) {
	return null;
}

TestCaseBase.prototype.getDepthAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return null;
}

TestCaseBase.prototype.getFSLAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return null;
}

TestCaseBase.prototype.getVelocityXAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return null;
}

TestCaseBase.prototype.getVelocityYAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return null;
}

TestCaseBase.prototype.getFrontPositionAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return null;
}

TestCaseBase.prototype.getFrontVelocityAtTime = function (domainSizeX, domainSizeY, domainResolution, simulationTime) {
	return null;
}

module.exports = TestCaseBase;
