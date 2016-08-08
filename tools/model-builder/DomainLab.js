'use strict';

var DomainBase = require('./DomainBase');
var rasterTools = require('./RasterTools');
var testCases = require('./TestCases');
var downloadTools = require('./DownloadTools');

const domainFiles = {
	'getTopography': 'TEST_DOMAIN_DTM.img',
	'getManningCoefficient': 'TEST_DOMAIN_MANNING.img',
	'getInitialDepth': 'TEST_DOMAIN_DEPTH.img',
	'getInitialFSL': 'TEST_DOMAIN_FSL.img',
	'getInitialVelocityX': 'TEST_DOMAIN_VELX.img',
	'getInitialVelocityY': 'TEST_DOMAIN_VELY.img'
};

const validationFiles = {
	'getDepthAtTime': 'TEST_VALIDATION_DEPTH_%t.img',
	'getFSLAtTime': 'TEST_VALIDATION_FSL_%t.img',
	'getVelocityXAtTime': 'TEST_VALIDATION_VELX_%t.img',
	'getVelocityYAtTime': 'TEST_VALIDATION_VELY_%t.img'
};

function DomainLab (parentModel, cb) {
	DomainBase.apply(this, Array.prototype.slice.call(arguments));
	
	this.testInstance = null;
	
	this.requiredFiles = {};
	this.supportFiles = {};
	for (let file in domainFiles) {
		this.requiredFiles[file] = null;
	}
	
	// Download dir might not exist yet -- force waiting for the download tools to spin up...
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
	
	var testClass = testCases.getTestByName(this.name);
	if (!testClass) {
		console.log('    Could not identify test case.');
		cb(false);
		return;
	}
	
	var testDefinition = new testClass(this);
	this.testInstance = testDefinition;
	
	console.log('     - - - - - - - - - - - - - - - - - -');
	console.log(testDefinition.getDescription());
	console.log('     - - - - - - - - - - - - - - - - - -');

	let domainExtent = testDefinition.getExtent() || this.parentModel.getExtent();
	let domainResolution = testDefinition.getResolution() || this.parentModel.getResolution();
	
	if (testDefinition.getManningCoefficient() !== null) {
		this.parentModel.overrideManningCoefficient(testDefinition.getManningCoefficient());
	}
	
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
			console.log('    This test provides a value for the file ' + domainTarget);
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
	
	for (let validationFile in validationFiles) {
		for (let time = this.parentModel.getOutputFrequency(); time <= this.parentModel.getDuration(); time += this.parentModel.getOutputFrequency()) {
			let domainData = testDefinition[validationFile].call(testDefinition, domainSizeX, domainSizeY, domainResolution, time);
			let domainTarget = validationFiles[validationFile].replace('%t', time.toString());
			if (domainData) {
				console.log('    This test provides an output validation file ' + domainTarget);
				fileCount++;
				rasterTools.arrayToRaster(
					downloadTools.getDirectoryPath() + domainTarget, 
					'HFA', 
					domainExtent,
					domainResolution, 
					domainData, 
					fileComplete
				);
				this.supportFiles[domainTarget] = downloadTools.getDirectoryPath() + domainTarget;
			}
		}
	}
}

DomainLab.prototype.getSupportFiles = function () {
	return this.supportFiles;
}

DomainLab.prototype.getPathTopography = function () {
	return this.requiredFiles['getTopography'];
}

DomainLab.prototype.getPathManningCoefficient = function () {
	return this.requiredFiles['getManningCoefficient'];
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
