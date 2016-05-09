'use strict';

var domain = require('./domain');

var model = function(name, targetDirectory, type, extent) {
	this.name = name;
	this.targetDirectory = targetDirectory;
	this.extent = extent;
	this.type = type;
	this.extent = extent;
	this.domain = null;
};

model.prototype.prepareModel = function() {
	this.domain = new domain(this.extent);
};

module.exports = model;
