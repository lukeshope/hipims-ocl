'use strict';

var rasterTools = require('./raster');

var domainLab = function(parentModel, cb) {
	this.parentModel = parentModel;
	this.extent = parentModel.getExtent();
	this.name = parentModel.getName();
	this.domainPrepare(cb);
};

domainLab.prototype.domainPrepare = function (cb) {
	console.log('--> Preparing domain data...');
	console.log('    Non-real world domain required: ' + this.name);
	
	cb(true);
}

domainLab.prototype.getPathTopography = function () {
	return null;
}

module.exports = domainLab;
