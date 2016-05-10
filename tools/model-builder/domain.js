'use strict';

var bngTile = require('./bngTile');
var rasterTools = require('./raster');
var downloadTools = require('./download');

var domain = function(extent, cb) {
	// TODO: Work out which tiles are required for the given extent
	this.extent = extent;
	this.tiles = extent.getBngTileNames();
	this.domainPrepare(cb);
};

domain.prototype.domainPrepare = function (cb) {
	console.log('--> Preparing domain data...');
	
	this.bngTiles = {};
	this.bngTileCount = this.tiles.length;
	this.bngTileDone = 0;
	this.clipDone = false;
	this.clipCallback = cb;
	
	this.tiles.forEach((tileName) => {
		this.bngTiles[tileName] = new bngTile(tileName);
		this.bngTiles[tileName].requireTile(this.domainPrepareTileFinished.bind(this));
	});
};

domain.prototype.domainPrepareTileFinished = function (tileName) {
	this.bngTileDone++;
	console.log('    Tile ' + tileName + ' required for the domain is now ready.');
	
	if (this.bngTileDone >= this.bngTileCount) {
		console.log('    All tiles required for the domain are now ready.');
		this.domainPrepareFinished();
	}
};

domain.prototype.domainPrepareFinished = function (tileName) {
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

domain.prototype.domainClip = function () {
	console.log('--> Preparing to clip the domain...');
	
	rasterTools.clipRaster(
		downloadTools.getDirectoryPath() + 'DOMAIN_DTM.vrt',
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

domain.prototype.domainClipFinished = function () {
	console.log('    Domain clipping is now complete.');
	if (this.clipCallback) this.clipCallback(true);
}

module.exports = domain;
