'use strict';

var fs = require('fs');
var request = require('request');
var glob = require('glob');
var path = require('path');

var rasterTools = require('./RasterTools');
var downloadTools = require('./DownloadTools');
var zipTools = require('./ZipTools');

const apiEndpointEA = 'http://www.geostore.com/environment-agency/rest/product/EA_SUPPLIED_OS_10KM/';
const apiDownloadEA = 'http://www.geostore.com/environment-agency/rest/product/download/';
const apiDownloadNRW = '';

const apiMatchEAFilenameDTM = /LIDAR-DTM-2M-[A-Z][A-Z][0-9][0-9]([a-z]{2})?\.zip/;
const apiMatchEAFilenameDEM = /LIDAR-DSM-2M-[A-Z][A-Z][0-9][0-9]([a-z]{2})?\.zip/;

function BngTile (tileName) {
	this.tileName = tileName;
	this.tileAssessed = false;
	this.tileDownloaded = false;
	this.tileExtracted = false;
	this.tileRasterised = false;
	this.tileRequired = false;
	this.tilePrepared = false;
	this.tileCallback = null;
	this.assessState();
};

BngTile.prototype.assessState = function () {
	var assessmentTotal = 3;
	var assessmentCount = 0;
	glob(downloadTools.getDirectoryPath() + this.tileName + '*_D?M_*.zip', (err, files) => {
		this.tileDownloaded = files.length >= 2; assessmentCount++; assessmentNext()
	});
	this.assessPaths([
		downloadTools.getDirectoryPath() + this.tileName + '_DTM/',
		downloadTools.getDirectoryPath() + this.tileName + '_DEM/'
	], (pathsFound) => { this.tileExtracted = pathsFound; assessmentCount++; assessmentNext() });
	this.assessPaths([
		downloadTools.getDirectoryPath() + this.tileName + '_DTM.vrt',
		downloadTools.getDirectoryPath() + this.tileName + '_DEM.vrt'
	], (pathsFound) => { this.tileRasterised = pathsFound; assessmentCount++; assessmentNext() });
	
	var assessmentNext = () => {
		if (assessmentCount < assessmentTotal) return;
		if (!this.tileDownloaded) {
			console.log('    Tile ' + this.tileName + ' required -- not yet downloaded.');
		} else if (!this.tileExtracted) {
			console.log('    Tile ' + this.tileName + ' required -- not yet extracted.');
		} else if (!this.tileRasterised) {
			console.log('    Tile ' + this.tileName + ' required -- not yet merged.');
		} else {
			console.log('    Tile ' + this.tileName + ' required -- already prepared.');
		}
		this.tileAssessed = true;
		this.prepareForUse();
	}
};

BngTile.prototype.assessPaths = function (paths, cb) {
	var returnCount = 0;
	var returnFail = false;
	paths.forEach((path) => {
		fs.access(path, fs.R_OK, (err) => {
			returnCount++;
			returnFail = returnFail | !!err;
			if (returnCount >= paths.length) {
				cb(!returnFail);
			}
		});
	});
};

BngTile.prototype.requireTile = function (cb) {
	this.tileRequired = true;
	this.tileCallback = cb;
	this.prepareForUse();
};

BngTile.prototype.prepareForUse = function () {
	if (!this.tileAssessed) return;
	
	if (!this.tileDownloaded) {
		this.download();
	} else if (!this.tileExtracted) {
		this.extract();
	} else if (!this.tileRasterised) {
		this.rasterise();
	} else {
		this.tilePrepared = true;
		if (this.tileCallback) {
			this.tileCallback(this.tileName);
		}
	}
};

BngTile.prototype.download = function () {
	console.log('    Attempting to download tile ' + this.tileName + '...');
	
	request(apiEndpointEA + this.tileName + '?catalogName=Survey', (e, r, b) => {
		if (r.statusCode !== 200) {
			console.log('    Request returned status code ' + r.statusCode + '.');
			return;
		}
		if (e) {
			console.log('    Request returned an error: ' + e);
			return;
		}
		this.downloadStartFiles(b, 'EA');
	});
};

BngTile.prototype.downloadStartFiles = function (jsonText, source) {
	var returnedDatasets = JSON.parse(jsonText);
	this.tileDownloadFilesTotal = 0;
	this.tileDownloadFilesFinished = 0;
	
	if (source === 'EA') {
		console.log('    Identified ' + returnedDatasets.length + ' LiDAR datasets for tile ' + this.tileName + '.');
		returnedDatasets.forEach((dataset) => {
			if (dataset.fileName.match(apiMatchEAFilenameDTM) !== null || dataset.fileName.match(apiMatchEAFilenameDEM) !== null) {
				this.tileDownloadFilesTotal++;
				console.log('    Dataset ' + dataset.guid + ' provides required data.');
				var eaTileName = dataset.fileName.match(/[A-Z]{2}[0-9]{2}([a-z]{2})?/)[0];
				downloadTools.pushToQueue(
					apiDownloadEA + dataset.guid,
					eaTileName + '_' + (dataset.fileName.match(apiMatchEAFilenameDTM) !== null ? 'DTM' : 'DEM') + '_' + source + '.zip',
					this.downloadEndFile.bind(this)
				);
			}
		});
	}
	
	if (this.tileDownloadFilesTotal === 0) {
		console.log('    Required LiDAR files could not be found.');
	}
};

BngTile.prototype.downloadEndFile = function (err, filename) {
	if (err !== null) {
		console.log('    An error occured downloading a file for tile ' + this.tileName + '.');
		return;
	}
	this.tileDownloadFilesFinished++;
	if (this.tileDownloadFilesFinished >= this.tileDownloadFilesTotal) {
		this.downloadFinished();
	}
};

BngTile.prototype.downloadFinished = function () {
	console.log('    All downloads have finished for tile ' + this.tileName + '.');
	this.tileDownloaded = true;
	this.prepareForUse();
};

BngTile.prototype.extract = function () {
	console.log('    Attempting to extract tile ' + this.tileName + '...');
	
	glob(downloadTools.getDirectoryPath() + this.tileName + '*_D?M_*.zip', (err, files) => {
		this.tileExtractFilesTotal = files.length;
		this.tileExtractFilesFinished = 0;
		
		var onExtracted = () => {
			this.tileExtractFilesFinished++
			if (this.tileExtractFilesFinished >= this.tileExtractFilesTotal) {
				this.tileExtracted = true;
				this.prepareForUse();
			}
		};
		
		files.forEach((file) => {
			zipTools.extract(
				path.basename(file),
				onExtracted,
                downloadTools.getDirectoryPath() + path.basename(file).replace(/[a-z]+/, '').replace(/_[A-Z]+\..+$/, '')
			);
		});
	});
};

BngTile.prototype.rasterise = function () {
	console.log('    Attempting to rasterise tile ' + this.tileName + '...');
	
	glob(downloadTools.getDirectoryPath() + this.tileName + '_D?M', (err, directories) => {
		this.tileRasteriseDirectoriesTotal = directories.length;
		this.tileRasteriseDirectoriesFinished = 0;
		
		directories.forEach((dir) => {
			glob(dir + '/*.asc', (err, files) => {
				rasterTools.mergeToVRT(
					downloadTools.getDirectoryPath() + path.basename(dir) + '.vrt',
					files,
					(err) => { 
						console.log('    Finished building ' + path.basename(dir) + ' VRT.'); 
						this.tileRasteriseDirectoriesFinished++;
						
						if (this.tileRasteriseDirectoriesFinished >= this.tileRasteriseDirectoriesTotal) {
							this.tileRasterised = true;
							this.prepareForUse();
						}
					}
				);
			});
		});
	});
};

module.exports = BngTile;
