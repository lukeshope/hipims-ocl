'use strict';

function TestCaseBase (parentDomain) {
	this.parentDomain = parentDomain;
};

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
