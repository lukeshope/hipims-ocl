'use strict';

var rasterTools = require('./RasterTools');
var testCases = require('./TestCases');

function DomainBase (parentModel, cb) {
	this.parentModel = parentModel;
	this.extent = parentModel ? parentModel.getExtent() : null;
	this.name = parentModel ? parentModel.getName() : null;
};

DomainBase.prototype.domainPrepare = function (cb) {
	console.log('--> Cannot prepare domain data...');
	console.log('    This type of domain does not have a prepare function.');
	cb(false);
}

DomainBase.prototype.getPathTopography = function () {
	return null;
}

DomainBase.prototype.getPathInitialDepth = function () {
	return null;
}

DomainBase.prototype.getPathInitialFSL = function () {
	return null;
}

DomainBase.prototype.getPathInitialVelocityX = function () {
	return null;
}

DomainBase.prototype.getPathInitialVelocityY = function () {
	return null;
}

module.exports = DomainBase;
