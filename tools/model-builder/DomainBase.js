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

DomainBase.prototype.getDataSources = function () {
	var domainFiles = [];
	var domainCount = this.parentModel.getDomainCount();
	
	function getSourceDefinition(type, value, filename, copyFrom) {
		var actions = [];
		if (copyFrom) {
			if (domainCount > 1) {
				actions.push({
					action: 'split',
					source: copyFrom
				});
			} else {
				actions.push({
					action: 'copy',
					source: copyFrom
				});
			}
		}
		return {
			type: type,
			value: value,
			source: filename,
			actions: actions
		};
	}
	
	if (this.getPathTopography()) {
		domainFiles.push(getSourceDefinition('raster', 'structure,dem', 'MODEL_TOPOGRAPHY_%d.img', this.getPathTopography()));
	} else {
		return false;
	}
	
	if (this.getPathInitialDepth()) {
		domainFiles.push(getSourceDefinition('raster', 'depth', 'MODEL_INITIAL_DEPTH_%d.img', this.getPathInitialDepth()));
	} else if (this.getPathInitialFSL()) {
		domainFiles.push(getSourceDefinition('raster', 'fsl', 'MODEL_INITIAL_FSL_%d.img', this.getPathInitialFSL()));
	} else {
		domainFiles.push(getSourceDefinition('constant', 'depth', '0.0'));
	}
	
	if (this.getPathInitialVelocityX()) {
		domainFiles.push(getSourceDefinition('raster', 'velocityX', 'MODEL_INITIAL_VEL_X_%d.img', this.getPathInitialVelocityX()));
	} else {
		domainFiles.push(getSourceDefinition('constant', 'velocityX', '0.0'));
	}
	
	if (this.getPathInitialVelocityY()) {
		domainFiles.push(getSourceDefinition('raster', 'velocityY', 'MODEL_INITIAL_VEL_Y_%d.img', this.getPathInitialVelocityY()));
	} else {
		domainFiles.push(getSourceDefinition('constant', 'velocityY', '0.0'));
	}
	
	return domainFiles;
}

DomainBase.prototype.getSupportFiles = function () {
	return {};
}

DomainBase.prototype.getPathTopography = function () {
	return null;
}

DomainBase.prototype.getPathManningCoefficient = function () {
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
