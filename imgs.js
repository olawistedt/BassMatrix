
  var Module = typeof Module !== 'undefined' ? Module : {};
  
  if (!Module.expectedDataFileDownloads) {
    Module.expectedDataFileDownloads = 0;
  }
  Module.expectedDataFileDownloads++;
  (function() {
   var loadPackage = function(metadata) {
  
      var PACKAGE_PATH;
      if (typeof window === 'object') {
        PACKAGE_PATH = window['encodeURIComponent'](window.location.pathname.toString().substring(0, window.location.pathname.toString().lastIndexOf('/')) + '/');
      } else if (typeof location !== 'undefined') {
        // worker
        PACKAGE_PATH = encodeURIComponent(location.pathname.toString().substring(0, location.pathname.toString().lastIndexOf('/')) + '/');
      } else {
        throw 'using preloaded data can only be done on a web page or in a web worker';
      }
      var PACKAGE_NAME = 'imgs.data';
      var REMOTE_PACKAGE_BASE = 'imgs.data';
      if (typeof Module['locateFilePackage'] === 'function' && !Module['locateFile']) {
        Module['locateFile'] = Module['locateFilePackage'];
        err('warning: you defined Module.locateFilePackage, that has been renamed to Module.locateFile (using your locateFilePackage for now)');
      }
      var REMOTE_PACKAGE_NAME = Module['locateFile'] ? Module['locateFile'](REMOTE_PACKAGE_BASE, '') : REMOTE_PACKAGE_BASE;
    
      var REMOTE_PACKAGE_SIZE = metadata['remote_package_size'];
      var PACKAGE_UUID = metadata['package_uuid'];
    
      function fetchRemotePackage(packageName, packageSize, callback, errback) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', packageName, true);
        xhr.responseType = 'arraybuffer';
        xhr.onprogress = function(event) {
          var url = packageName;
          var size = packageSize;
          if (event.total) size = event.total;
          if (event.loaded) {
            if (!xhr.addedTotal) {
              xhr.addedTotal = true;
              if (!Module.dataFileDownloads) Module.dataFileDownloads = {};
              Module.dataFileDownloads[url] = {
                loaded: event.loaded,
                total: size
              };
            } else {
              Module.dataFileDownloads[url].loaded = event.loaded;
            }
            var total = 0;
            var loaded = 0;
            var num = 0;
            for (var download in Module.dataFileDownloads) {
            var data = Module.dataFileDownloads[download];
              total += data.total;
              loaded += data.loaded;
              num++;
            }
            total = Math.ceil(total * Module.expectedDataFileDownloads/num);
            if (Module['setStatus']) Module['setStatus']('Downloading data... (' + loaded + '/' + total + ')');
          } else if (!Module.dataFileDownloads) {
            if (Module['setStatus']) Module['setStatus']('Downloading data...');
          }
        };
        xhr.onerror = function(event) {
          throw new Error("NetworkError for: " + packageName);
        }
        xhr.onload = function(event) {
          if (xhr.status == 200 || xhr.status == 304 || xhr.status == 206 || (xhr.status == 0 && xhr.response)) { // file URLs can return 0
            var packageData = xhr.response;
            callback(packageData);
          } else {
            throw new Error(xhr.statusText + " : " + xhr.responseURL);
          }
        };
        xhr.send(null);
      };

      function handleError(error) {
        console.error('package error:', error);
      };
    
    function runWithFS() {
  
      function assert(check, msg) {
        if (!check) throw msg + new Error().stack;
      }
  Module['FS_createPath']("/", "resources", true, true);
Module['FS_createPath']("/resources", "img", true, true);

          /** @constructor */
          function DataRequest(start, end, audio) {
            this.start = start;
            this.end = end;
            this.audio = audio;
          }
          DataRequest.prototype = {
            requests: {},
            open: function(mode, name) {
              this.name = name;
              this.requests[name] = this;
              Module['addRunDependency']('fp ' + this.name);
            },
            send: function() {},
            onload: function() {
              var byteArray = this.byteArray.subarray(this.start, this.end);
              this.finish(byteArray);
            },
            finish: function(byteArray) {
              var that = this;
      
          Module['FS_createPreloadedFile'](this.name, null, byteArray, true, true, function() {
            Module['removeRunDependency']('fp ' + that.name);
          }, function() {
            if (that.audio) {
              Module['removeRunDependency']('fp ' + that.name); // workaround for chromium bug 124926 (still no audio with this, but at least we don't hang)
            } else {
              err('Preloading file ' + that.name + ' failed');
            }
          }, false, true); // canOwn this data in the filesystem, it is a slide into the heap that will never change
  
              this.requests[this.name] = null;
            }
          };
      
              var files = metadata['files'];
              for (var i = 0; i < files.length; ++i) {
                new DataRequest(files[i]['start'], files[i]['end'], files[i]['audio']).open('GET', files[i]['filename']);
              }
      
        
        var indexedDB;
        if (typeof window === 'object') {
          indexedDB = window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
        } else if (typeof location !== 'undefined') {
          // worker
          indexedDB = self.indexedDB;
        } else {
          throw 'using IndexedDB to cache data can only be done on a web page or in a web worker';
        }
        var IDB_RO = "readonly";
        var IDB_RW = "readwrite";
        var DB_NAME = "/";
        var DB_VERSION = 1;
        var METADATA_STORE_NAME = 'METADATA';
        var PACKAGE_STORE_NAME = 'PACKAGES';
        function openDatabase(callback, errback) {
          try {
            var openRequest = indexedDB.open(DB_NAME, DB_VERSION);
          } catch (e) {
            return errback(e);
          }
          openRequest.onupgradeneeded = function(event) {
            var db = event.target.result;

            if(db.objectStoreNames.contains(PACKAGE_STORE_NAME)) {
              db.deleteObjectStore(PACKAGE_STORE_NAME);
            }
            var packages = db.createObjectStore(PACKAGE_STORE_NAME);

            if(db.objectStoreNames.contains(METADATA_STORE_NAME)) {
              db.deleteObjectStore(METADATA_STORE_NAME);
            }
            var metadata = db.createObjectStore(METADATA_STORE_NAME);
          };
          openRequest.onsuccess = function(event) {
            var db = event.target.result;
            callback(db);
          };
          openRequest.onerror = function(error) {
            errback(error);
          };
        };

        // This is needed as chromium has a limit on per-entry files in IndexedDB
        // https://cs.chromium.org/chromium/src/content/renderer/indexed_db/webidbdatabase_impl.cc?type=cs&sq=package:chromium&g=0&l=177
        // https://cs.chromium.org/chromium/src/out/Debug/gen/third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h?type=cs&sq=package:chromium&g=0&l=60
        // We set the chunk size to 64MB to stay well-below the limit
        var CHUNK_SIZE = 64 * 1024 * 1024;

        function cacheRemotePackage(
          db,
          packageName,
          packageData,
          packageMeta,
          callback,
          errback
        ) {
          var transactionPackages = db.transaction([PACKAGE_STORE_NAME], IDB_RW);
          var packages = transactionPackages.objectStore(PACKAGE_STORE_NAME);
          var chunkSliceStart = 0;
          var nextChunkSliceStart = 0;
          var chunkCount = Math.ceil(packageData.byteLength / CHUNK_SIZE);
          var finishedChunks = 0;
          for (var chunkId = 0; chunkId < chunkCount; chunkId++) {
            nextChunkSliceStart += CHUNK_SIZE;
            var putPackageRequest = packages.put(
              packageData.slice(chunkSliceStart, nextChunkSliceStart),
              'package/' + packageName + '/' + chunkId
            );
            chunkSliceStart = nextChunkSliceStart;
            putPackageRequest.onsuccess = function(event) {
              finishedChunks++;
              if (finishedChunks == chunkCount) {
                var transaction_metadata = db.transaction(
                  [METADATA_STORE_NAME],
                  IDB_RW
                );
                var metadata = transaction_metadata.objectStore(METADATA_STORE_NAME);
                var putMetadataRequest = metadata.put(
                  {
                    'uuid': packageMeta.uuid,
                    'chunkCount': chunkCount
                  },
                  'metadata/' + packageName
                );
                putMetadataRequest.onsuccess = function(event) {
                  callback(packageData);
                };
                putMetadataRequest.onerror = function(error) {
                  errback(error);
                };
              }
            };
            putPackageRequest.onerror = function(error) {
              errback(error);
            };
          }
        }

        /* Check if there's a cached package, and if so whether it's the latest available */
        function checkCachedPackage(db, packageName, callback, errback) {
          var transaction = db.transaction([METADATA_STORE_NAME], IDB_RO);
          var metadata = transaction.objectStore(METADATA_STORE_NAME);
          var getRequest = metadata.get('metadata/' + packageName);
          getRequest.onsuccess = function(event) {
            var result = event.target.result;
            if (!result) {
              return callback(false, null);
            } else {
              return callback(PACKAGE_UUID === result['uuid'], result);
            }
          };
          getRequest.onerror = function(error) {
            errback(error);
          };
        }

        function fetchCachedPackage(db, packageName, metadata, callback, errback) {
          var transaction = db.transaction([PACKAGE_STORE_NAME], IDB_RO);
          var packages = transaction.objectStore(PACKAGE_STORE_NAME);

          var chunksDone = 0;
          var totalSize = 0;
          var chunkCount = metadata['chunkCount'];
          var chunks = new Array(chunkCount);

          for (var chunkId = 0; chunkId < chunkCount; chunkId++) {
            var getRequest = packages.get('package/' + packageName + '/' + chunkId);
            getRequest.onsuccess = function(event) {
              // If there's only 1 chunk, there's nothing to concatenate it with so we can just return it now
              if (chunkCount == 1) {
                callback(event.target.result);
              } else {
                chunksDone++;
                totalSize += event.target.result.byteLength;
                chunks.push(event.target.result);
                if (chunksDone == chunkCount) {
                  if (chunksDone == 1) {
                    callback(event.target.result);
                  } else {
                    var tempTyped = new Uint8Array(totalSize);
                    var byteOffset = 0;
                    for (var chunkId in chunks) {
                      var buffer = chunks[chunkId];
                      tempTyped.set(new Uint8Array(buffer), byteOffset);
                      byteOffset += buffer.byteLength;
                      buffer = undefined;
                    }
                    chunks = undefined;
                    callback(tempTyped.buffer);
                    tempTyped = undefined;
                  }
                }
              }
            };
            getRequest.onerror = function(error) {
              errback(error);
            };
          }
        }
      
      function processPackageData(arrayBuffer) {
        assert(arrayBuffer, 'Loading data file failed.');
        assert(arrayBuffer instanceof ArrayBuffer, 'bad input to processPackageData');
        var byteArray = new Uint8Array(arrayBuffer);
        var curr;
        
          // Reuse the bytearray from the XHR as the source for file reads.
          DataRequest.prototype.byteArray = byteArray;
    
            var files = metadata['files'];
            for (var i = 0; i < files.length; ++i) {
              DataRequest.prototype.requests[files[i].filename].onload();
            }
                Module['removeRunDependency']('datafile_imgs.data');

      };
      Module['addRunDependency']('datafile_imgs.data');
    
      if (!Module.preloadResults) Module.preloadResults = {};
    
        function preloadFallback(error) {
          console.error(error);
          console.error('falling back to default preload behavior');
          fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE, processPackageData, handleError);
        };

        openDatabase(
          function(db) {
            checkCachedPackage(db, PACKAGE_PATH + PACKAGE_NAME,
              function(useCached, metadata) {
                Module.preloadResults[PACKAGE_NAME] = {fromCache: useCached};
                if (useCached) {
                  fetchCachedPackage(db, PACKAGE_PATH + PACKAGE_NAME, metadata, processPackageData, preloadFallback);
                } else {
                  fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE,
                    function(packageData) {
                      cacheRemotePackage(db, PACKAGE_PATH + PACKAGE_NAME, packageData, {uuid:PACKAGE_UUID}, processPackageData,
                        function(error) {
                          console.error(error);
                          processPackageData(packageData);
                        });
                    }
                  , preloadFallback);
                }
              }
            , preloadFallback);
          }
        , preloadFallback);

        if (Module['setStatus']) Module['setStatus']('Downloading...');
      
    }
    if (Module['calledRun']) {
      runWithFS();
    } else {
      if (!Module['preRun']) Module['preRun'] = [];
      Module["preRun"].push(runWithFS); // FS is not initialized yet, wait for it
    }
  
   }
   loadPackage({"files": [{"filename": "/resources/img/btnPatA.png", "start": 0, "end": 7601, "audio": 0}, {"filename": "/resources/img/btnLed.png", "start": 7601, "end": 12280, "audio": 0}, {"filename": "/resources/img/btnSeq.png", "start": 12280, "end": 14313, "audio": 0}, {"filename": "/resources/img/background-1130x840.png", "start": 14313, "end": 418337, "audio": 0}, {"filename": "/resources/img/background-1130x840.xcf", "start": 418337, "end": 1467952, "audio": 0}, {"filename": "/resources/img/btnPatC.png", "start": 1467952, "end": 1475551, "audio": 0}, {"filename": "/resources/img/btnPatG.png", "start": 1475551, "end": 1483178, "audio": 0}, {"filename": "/resources/img/btnPatternOctav2.png", "start": 1483178, "end": 1495660, "audio": 0}, {"filename": "/resources/img/606-2.knob", "start": 1495660, "end": 1584128, "audio": 0}, {"filename": "/resources/img/btnPatC#.png", "start": 1584128, "end": 1591991, "audio": 0}, {"filename": "/resources/img/btnMidi.png", "start": 1591991, "end": 1604438, "audio": 0}, {"filename": "/resources/img/btnPatMod.knob", "start": 1604438, "end": 3980121, "audio": 0}, {"filename": "/resources/img/fx-1-100x100.png", "start": 3980121, "end": 4976716, "audio": 0}, {"filename": "/resources/img/btnStop.png", "start": 4976716, "end": 4987722, "audio": 0}, {"filename": "/resources/img/btnAccent.bmp", "start": 4987722, "end": 5012352, "audio": 0}, {"filename": "/resources/img/btnPtnClear.png", "start": 5012352, "end": 5024133, "audio": 0}, {"filename": "/resources/img/606-2-ola-1.png", "start": 5024133, "end": 5037027, "audio": 0}, {"filename": "/resources/img/btnProp.png", "start": 5037027, "end": 5039014, "audio": 0}, {"filename": "/resources/img/btnPatternOctav3.png", "start": 5039014, "end": 5051789, "audio": 0}, {"filename": "/resources/img/btnProp-Brons.png", "start": 5051789, "end": 5053861, "audio": 0}, {"filename": "/resources/img/knobLoopSize.knob", "start": 5053861, "end": 5146359, "audio": 0}, {"filename": "/resources/img/btnC.png", "start": 5146359, "end": 5149264, "audio": 0}, {"filename": "/resources/img/ola-background-1130x840.xcf", "start": 5149264, "end": 13406857, "audio": 0}, {"filename": "/resources/img/btnProp-Turkos.png", "start": 13406857, "end": 13408844, "audio": 0}, {"filename": "/resources/img/btnPatA#.png", "start": 13408844, "end": 13416711, "audio": 0}, {"filename": "/resources/img/btnPatE.png", "start": 13416711, "end": 13422350, "audio": 0}, {"filename": "/resources/img/btnAccent.knob", "start": 13422350, "end": 13463226, "audio": 0}, {"filename": "/resources/img/606-2.png", "start": 13463226, "end": 14715857, "audio": 0}, {"filename": "/resources/img/btnPatF#.png", "start": 14715857, "end": 14723587, "audio": 0}, {"filename": "/resources/img/btnLed.knob", "start": 14723587, "end": 14764710, "audio": 0}, {"filename": "/resources/img/knobLoopSize.png", "start": 14764710, "end": 14885572, "audio": 0}, {"filename": "/resources/img/btnPtnCopy.png", "start": 14885572, "end": 14896667, "audio": 0}, {"filename": "/resources/img/btnPatD#.png", "start": 14896667, "end": 14904454, "audio": 0}, {"filename": "/resources/img/btnPatF.png", "start": 14904454, "end": 14911885, "audio": 0}, {"filename": "/resources/img/btnPatG#.png", "start": 14911885, "end": 14919767, "audio": 0}, {"filename": "/resources/img/btnWaveForm.png", "start": 14919767, "end": 14926616, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.knob", "start": 14926616, "end": 15025038, "audio": 0}, {"filename": "/resources/img/btnEffects.png", "start": 15025038, "end": 15033125, "audio": 0}, {"filename": "/resources/img/o303-1130x840.png", "start": 15033125, "end": 15709687, "audio": 0}, {"filename": "/resources/img/btnKeySync.png", "start": 15709687, "end": 15722372, "audio": 0}, {"filename": "/resources/img/btnInternalSync.png", "start": 15722372, "end": 15734829, "audio": 0}, {"filename": "/resources/img/btnSeq.knob", "start": 15734829, "end": 18121236, "audio": 0}, {"filename": "/resources/img/btnHostSync.knob", "start": 18121236, "end": 20496391, "audio": 0}, {"filename": "/resources/img/btnPatternOctav.knob", "start": 20496391, "end": 22878044, "audio": 0}, {"filename": "/resources/img/btnPtnRand.png", "start": 22878044, "end": 22890578, "audio": 0}, {"filename": "/resources/img/btnHostSync.png", "start": 22890578, "end": 22903441, "audio": 0}, {"filename": "/resources/img/fx1-1.png", "start": 22903441, "end": 25217091, "audio": 0}, {"filename": "/resources/img/btnC.knob", "start": 25217091, "end": 27598213, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.png", "start": 27598213, "end": 28704371, "audio": 0}, {"filename": "/resources/img/btnProp-Blue.png", "start": 28704371, "end": 28706367, "audio": 0}, {"filename": "/resources/img/o303.png", "start": 28706367, "end": 29425562, "audio": 0}, {"filename": "/resources/img/btnPatD.png", "start": 29425562, "end": 29433086, "audio": 0}, {"filename": "/resources/img/btnEffects.xcf", "start": 29433086, "end": 29445188, "audio": 0}, {"filename": "/resources/img/btnPatB.png", "start": 29445188, "end": 29452745, "audio": 0}], "remote_package_size": 29452745, "package_uuid": "82ade79e-85a9-4b68-82d9-5fddd048900d"});
  
  })();
  
