'use strict';

var DomainBase = require('./DomainBase');
var rasterTools = require('./RasterTools');
var testCases = require('./TestCases');
var downloadTools = require('./DownloadTools');

const domainFiles = {
	'getTopography': 'TEST_DOMAIN_DTM.img',
	'getInitialDepth': 'TEST_DOMAIN_DEPTH.img',
	'getInitialFSL': 'TEST_DOMAIN_FSL.img',
	'getInitialVelocityX': 'TEST_DOMAIN_VELX.img',
	'getInitialVelocityY': 'TEST_DOMAIN_VELY.img'
};

function DomainLab (parentModel, cb) {
	DomainBase.apply(this, Array.prototype.slice.call(arguments));
	
	this.requiredFiles = {};
	for (let file in domainFiles) {
		this.requiredFiles[file] = null;
	}
	
	// Download dir might not exist yet -- force waiting...
	downloadTools.pushToQueue(
		null,
		null,
		function () { this.domainPrepare(cb) }.bind(this)
	);
};
DomainLab.prototype = new DomainBase();

DomainLab.prototype.domainPrepare = function (cb) {
	console.log('--> Preparing domain data...');
	console.log('    Non-real world domain required: ' + this.name);
	
	var testDefinition = testCases.getTestByName(this.name);
	if (!testDefinition) {
		console.log('    Could not identify test case.');
		cb(false);
		return;
	}
	
	console.log('     - - - - - - - - - - - - - - - - - -');
	console.log(testDefinition.getDescription());
	console.log('     - - - - - - - - - - - - - - - - - -');

	let domainExtent = testDefinition.getExtent() || this.parentModel.getExtent();
	let domainResolution = testDefinition.getResolution() || this.parentModel.getResolution();
	
	domainExtent.snapToGrid(domainResolution);
	let domainSizeX = domainExtent.getSizeX(domainResolution);
	let domainSizeY = domainExtent.getSizeY(domainResolution);
	
	let fileCount = 0;
	let fileSatisfied = 0;
	var fileComplete = function (success) {
		if (!success) {
			console.log('    Could not generate a domain file.');
			cb(false);
			return;
		}
		fileSatisfied++;
		if (fileSatisfied >= fileCount) {
			cb(true);
		}
	};
	
	for (let domainFile in domainFiles) {
		let domainData = testDefinition[domainFile].call(testDefinition, domainSizeX, domainSizeY, domainResolution);
		let domainTarget = domainFiles[domainFile];
		if (domainData) {
			console.log('    This test provides a value for the fle ' + domainTarget);
			fileCount++;
			rasterTools.arrayToRaster(
				downloadTools.getDirectoryPath() + domainTarget, 
				'HFA', 
				domainExtent,
				domainResolution, 
				domainData, 
				fileComplete
			);
			this.requiredFiles[domainFile] = downloadTools.getDirectoryPath() + domainTarget;
		}
	}
}

DomainLab.prototype.getPathTopography = function () {
	return this.requiredFiles['getTopography'];
}

DomainLab.prototype.getPathInitialDepth = function () {
	return this.requiredFiles['getInitialDepth'];
}

DomainLab.prototype.getPathInitialFSL = function () {
	return this.requiredFiles['getInitialFSL'];
}

DomainLab.prototype.getPathInitialVelocityX = function () {
	return this.requiredFiles['getInitialVelocityX'];
}

DomainLab.prototype.getPathInitialVelocityY = function () {
	return this.requiredFiles['getInitialVelocityY'];
}

module.exports = DomainLab;
