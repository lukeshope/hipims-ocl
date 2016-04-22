'use strict';

var fs = require('fs');
var unzip = require('unzip');
var downloadTools = require('./download');

var zipTools = function() {
	// ...
};

zipTools.prototype.extract = function (archiveName, cb, targetDir) {
	if ( targetDir === undefined ) {
		targetDir = downloadTools.getDirectoryPath() + archiveName.replace(/_[A-Z]+\..+$/, '');
	}
	console.log('Extracting ' + archiveName + ' to directory...');
	fs.createReadStream(downloadTools.getDirectoryPath() + archiveName)
	  .pipe(
		unzip.Extract({ 
			path: targetDir
		})
		.on('close', () => {
			console.log('Finished extracting ' + archiveName + '.');
			if (cb) cb();
		})
	  );
};

var thisInstance = null;
module.exports = function () {
	if ( !thisInstance ) {
		thisInstance = new zipTools();
	}
	return thisInstance;
}();
