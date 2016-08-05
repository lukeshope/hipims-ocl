'use strict';

function TestCaseBase () {

};

TestCaseBase.prototype.getExtent = function () {
	return null;
}

TestCaseBase.prototype.getResolution = function () {
	return null;
}

TestCaseBase.prototype.getTopography = function (domainSizeX, domainSizeY, domainResolution) {
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

module.exports = TestCaseBase;
