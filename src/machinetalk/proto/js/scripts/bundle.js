#!/usr/bin/env node
var path = require('path');
var fs = require('fs');
var mkdirp = require('mkdirp');
var browserify = require('browserify')
var exec = require('child_process').exec;
var destinationPath = path.join(__dirname, '../../dist');
var inputFile = path.join(__dirname, '../../build/js/protoexport.js');

function makeBundle() {
    var bundler = new browserify({standalone: 'machinetalk.protobuf'});
    bundler.add(inputFile);

    bundler.bundle(function (err, src, map) {
        if (err) {
            console.log('Error:', err);
        }

        mkdirp.sync(destinationPath);
        fs.writeFileSync(path.join(destinationPath, 'machinetalk-protobuf.js'), src);

        console.log('bundle created');
    });
}

function makeMinBundle() {
    // As of browserify 5, you must enable debug mode in the constructor to use minifyify
    var bundler = new browserify({debug: true, standalone: 'machinetalk.protobuf'});
    bundler.add(inputFile);
    bundler.plugin('minifyify', {map: 'machinetalk-protobuf.min.map.json'});

    bundler.bundle(function (err, src, map) {
        if (err) {
            console.log('Error:', err);
        }

        mkdirp.sync(destinationPath);
        fs.writeFileSync(path.join(destinationPath, 'machinetalk-protobuf.min.js'), src);
        fs.writeFileSync(path.join(destinationPath, 'machinetalk-protobuf.min.map.json'), map);

        console.log('minified bundle created');

        makeMinGzBundle(path.join(destinationPath, 'machinetalk-protobuf.min.js'));
    });
}

function makeMinGzBundle(file) {
    var cmd = 'gzip -k -f -9 ' + file;

    exec(cmd, function(err, stdout, stderr) {
        if (err) {
            console.log('Error:', err);
        }

        console.log('Gzipped bundle created');
    });
}

makeBundle();
makeMinBundle();
