'use strict';

var DomainBase = require('./DomainBase');
var BngTile = require('./BngTile');
var rasterTools = require('./RasterTools');
var downloadTools = require('./DownloadTools');

function DomainBNG (parentModel, cb) {
	DomainBase.apply(this, Array.prototype.slice.call(arguments));
	this.tiles = this.extent.getBngTileNames();
	this.domainPrepare(cb);
};
DomainBNG.prototype = new DomainBase();

DomainBNG.prototype.domainPrepare = function (cb) {
	console.log('--> Preparing domain data...');
	
	this.bngTiles = {};
	this.bngTileCount = this.tiles.length;
	this.bngTileDone = 0;
	this.clipDone = false;
	this.clipCallback = cb;
	
	this.tiles.forEach((tileName) => {
		this.bngTiles[tileName] = new BngTile(tileName);
		this.bngTiles[tileName].requireTile(this.domainPrepareTileFinished.bind(this));
	});
};

DomainBNG.prototype.domainPrepareTileFinished = function (tileName) {
	this.bngTileDone++;
	console.log('    Tile ' + tileName + ' required for the domain is now ready.');
	
	if (this.bngTileDone >= this.bngTileCount) {
		console.log('    All tiles required for the domain are now ready.');
		this.domainPrepareFinished();
	}
};

DomainBNG.prototype.domainPrepareFinished = function (tileName) {
	// Make a single VRT for the DTM and the DEM
	rasterTools.mergeToVRT(
		downloadTools.getDirectoryPath() + 'DOMAIN_DTM.vrt',
		this.tiles.map((t) => { return downloadTools.getDirectoryPath() + t + '_DTM.vrt' }),
		(err) => {
			if (err) {
				console.log('    Error creating a single DTM VRT for the whole domain area.');
			} else {
				console.log('    Whole domain DTM is now ready.');
				this.domainReadyDTM = true;
				if (this.domainReadyDEM && this.domainReadyDTM) this.domainClip();
			}
		}
	);
	rasterTools.mergeToVRT(
		downloadTools.getDirectoryPath() + 'DOMAIN_DEM.vrt',
		this.tiles.map((t) => { return downloadTools.getDirectoryPath() + t + '_DEM.vrt' }),
		(err) => {
			if (err) {
				console.log('    Error creating a single DEM VRT for the whole domain area.');
			} else {
				console.log('    Whole domain DEM is now ready.');
				this.domainReadyDEM = true;
				if (this.domainReadyDEM && this.domainReadyDTM) this.domainClip();
			}
		}
	);
};

DomainBNG.prototype.domainClip = function () {
	console.log('--> Preparing to clip the domain...');
	
	rasterTools.clipRaster(
		downloadTools.getDirectoryPath() + 'DOMAIN_DEM.vrt',
		downloadTools.getDirectoryPath() + 'CLIP_DTM.img',
		'HFA',
		this.extent,
		(success) => {
			if (success) {
				this.clipDone = true;
				this.domainClipFinished();
			} else {
				console.log('    An error occured clipping the domain.');
				if (this.clipCallback) this.clipCallback(false);
			}
		}
	);
}

DomainBNG.prototype.domainClipFinished = function () {
	console.log('    Domain clipping is now complete.');
	if (this.clipCallback) this.clipCallback(true);
}

DomainBNG.prototype.getPathTopography = function () {
	return downloadTools.getDirectoryPath() + 'CLIP_DTM.img';
}

module.exports = DomainBNG;
