'use strict';

const fs = require('fs');
const http = require('http');
const downloadDir = './download';

function DownloadTools () {
	// Store a queue
	this.downloadReady = false;
	this.downloadQueue = [];
	this.downloadItem = function(url, filename, cb) { return {
		url: url,
		filename: filename,
		cb: cb
	}};

	// Need a download directory
	fs.access(downloadDir, fs.R_OK | fs.W_OK, (err) => {
		if (err) {
			console.log('    No download directory exists -- need to make one...');
			this.createDirectory();
		} else {
			console.log('    Download directory found.');
			this.downloadReady = true;
			this.processQueue();
		}
	});
};

DownloadTools.prototype.createDirectory = function () {
	fs.mkdir(downloadDir, (err) => {
		console.log('    ' + (!err ? 'Made' : 'Could not make') + ' downloads directory.');
		this.downloadReady = !err;
		this.processQueue();
	});
};

DownloadTools.prototype.emptyDirectory = function () {
	// ...TODO
};

DownloadTools.prototype.getDirectoryPath = function () {
	return downloadDir + '/';
};

DownloadTools.prototype.pushToQueue = function(url, filename, cb) {
	this.downloadQueue.push(this.downloadItem(url, filename, cb));
	this.processQueue();
};

DownloadTools.prototype.processQueue = function() {
	if (!this.downloadReady) return;
	if (!this.downloadQueue.length) return;
	this.fetchToFile(this.downloadQueue.splice(0, 1)[0]);
};

DownloadTools.prototype.fetchToFile = function (queueItem) {
	if (!queueItem.filename && !queueItem.url && queueItem.cb) {
		queueItem.cb(null, null);
		return;
	}	

	const targetPath = downloadDir + '/' + queueItem.filename;
	var file = fs.createWriteStream(targetPath);
	console.log('    Downloading ' + queueItem.filename + '...');

	var request = http.get(queueItem.url, (response) => {
		response.pipe(file);
		file.on('finish', () => {
			file.close();
			console.log('    Finished downloading ' + queueItem.filename + '.');
			if (queueItem.cb) queueItem.cb(null, queueItem.filename);
			this.processQueue();
		});
	}).on('error', (err) => {
		fs.unlink(targetPath);
		console.log('    An error occured downloading ' + queueItem.filename + ' -- file deleted.');
		if (cb) cb(err, filename);
	});
};

var thisInstance = null;
module.exports = function () {
	if ( !thisInstance ) {
		thisInstance = new DownloadTools();
	}
	return thisInstance;
}();
