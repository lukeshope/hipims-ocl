'use strict';

function TestCaseBase (parentDomain) {
	this.parentDomain = parentDomain;
};

TestCaseBase.prototype.getGridUsingFormula = function (domainSizeX, domainSizeY, domainResolution, cellFormula, timeValue) {
	let domainData = new Float32Array(domainSizeX * domainSizeY);

	// Reverse the direction of the Y-axis for these tests to match our LL origin
	for (let x = 0; x < domainSizeX; x++ ) {
		for (let y = 0; y < domainSizeY; y++) {
			let x0 = (x - Math.round(domainSizeX / 2)) * domainResolution;
			let y0 = ((domainSizeY - y) - Math.round(domainSizeY / 2)) * domainResolution;
			let a = y * domainSizeX + x;
			domainData[a] = cellFormula.call(this, x0, y0, timeValue);
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

TestCaseBase.prototype.getTopography = function (domainSizeX, domainSizeY, domainResolution) {
	return null;
}

TestCaseBase.prototype.getManningCoefficient = function (domainSizeX, domainSizeY, domainResolution) {
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

module.exports = TestCaseBase;
