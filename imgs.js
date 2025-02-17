
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
   loadPackage({"files": [{"filename": "/resources/img/btnPatA.png", "start": 0, "end": 7601, "audio": 0}, {"filename": "/resources/img/btnLed.png", "start": 7601, "end": 12280, "audio": 0}, {"filename": "/resources/img/btnSeq.png", "start": 12280, "end": 14313, "audio": 0}, {"filename": "/resources/img/background-1130x840.png", "start": 14313, "end": 425588, "audio": 0}, {"filename": "/resources/img/background-1130x840.xcf", "start": 425588, "end": 1475156, "audio": 0}, {"filename": "/resources/img/btnPatC.png", "start": 1475156, "end": 1482755, "audio": 0}, {"filename": "/resources/img/btnPatG.png", "start": 1482755, "end": 1490382, "audio": 0}, {"filename": "/resources/img/btnPatternOctav2.png", "start": 1490382, "end": 1502864, "audio": 0}, {"filename": "/resources/img/606-2.knob", "start": 1502864, "end": 1591332, "audio": 0}, {"filename": "/resources/img/btnPatC#.png", "start": 1591332, "end": 1599195, "audio": 0}, {"filename": "/resources/img/btnMidi.png", "start": 1599195, "end": 1611642, "audio": 0}, {"filename": "/resources/img/btnPatMod.knob", "start": 1611642, "end": 3987325, "audio": 0}, {"filename": "/resources/img/fx-1-100x100.png", "start": 3987325, "end": 4983920, "audio": 0}, {"filename": "/resources/img/btnStop.png", "start": 4983920, "end": 4994926, "audio": 0}, {"filename": "/resources/img/btnAccent.bmp", "start": 4994926, "end": 5019556, "audio": 0}, {"filename": "/resources/img/btnPtnClear.png", "start": 5019556, "end": 5031337, "audio": 0}, {"filename": "/resources/img/606-2-ola-1.png", "start": 5031337, "end": 5044231, "audio": 0}, {"filename": "/resources/img/btnProp.png", "start": 5044231, "end": 5046218, "audio": 0}, {"filename": "/resources/img/btnPatternOctav3.png", "start": 5046218, "end": 5058993, "audio": 0}, {"filename": "/resources/img/btnProp-Brons.png", "start": 5058993, "end": 5061065, "audio": 0}, {"filename": "/resources/img/knobLoopSize.knob", "start": 5061065, "end": 5153563, "audio": 0}, {"filename": "/resources/img/btnC.png", "start": 5153563, "end": 5156468, "audio": 0}, {"filename": "/resources/img/ola-background-1130x840.xcf", "start": 5156468, "end": 13414061, "audio": 0}, {"filename": "/resources/img/btnProp-Turkos.png", "start": 13414061, "end": 13416048, "audio": 0}, {"filename": "/resources/img/btnPatA#.png", "start": 13416048, "end": 13423915, "audio": 0}, {"filename": "/resources/img/btnPatE.png", "start": 13423915, "end": 13429554, "audio": 0}, {"filename": "/resources/img/btnAccent.knob", "start": 13429554, "end": 13470430, "audio": 0}, {"filename": "/resources/img/606-2.png", "start": 13470430, "end": 14723061, "audio": 0}, {"filename": "/resources/img/btnPatF#.png", "start": 14723061, "end": 14730791, "audio": 0}, {"filename": "/resources/img/btnLed.knob", "start": 14730791, "end": 14771914, "audio": 0}, {"filename": "/resources/img/knobLoopSize.png", "start": 14771914, "end": 14892776, "audio": 0}, {"filename": "/resources/img/btnPtnCopy.png", "start": 14892776, "end": 14903871, "audio": 0}, {"filename": "/resources/img/btnPatD#.png", "start": 14903871, "end": 14911658, "audio": 0}, {"filename": "/resources/img/btnPatF.png", "start": 14911658, "end": 14919089, "audio": 0}, {"filename": "/resources/img/btnPatG#.png", "start": 14919089, "end": 14926971, "audio": 0}, {"filename": "/resources/img/btnWaveForm.png", "start": 14926971, "end": 14933820, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.knob", "start": 14933820, "end": 15032242, "audio": 0}, {"filename": "/resources/img/o303-1130x840.png", "start": 15032242, "end": 15708804, "audio": 0}, {"filename": "/resources/img/btnKeySync.png", "start": 15708804, "end": 15721489, "audio": 0}, {"filename": "/resources/img/btnInternalSync.png", "start": 15721489, "end": 15733946, "audio": 0}, {"filename": "/resources/img/btnSeq.knob", "start": 15733946, "end": 18120353, "audio": 0}, {"filename": "/resources/img/btnHostSync.knob", "start": 18120353, "end": 20495508, "audio": 0}, {"filename": "/resources/img/btnPatternOctav.knob", "start": 20495508, "end": 22877161, "audio": 0}, {"filename": "/resources/img/btnPtnRand.png", "start": 22877161, "end": 22889695, "audio": 0}, {"filename": "/resources/img/btnHostSync.png", "start": 22889695, "end": 22902558, "audio": 0}, {"filename": "/resources/img/fx1-1.png", "start": 22902558, "end": 25216208, "audio": 0}, {"filename": "/resources/img/btnC.knob", "start": 25216208, "end": 27597330, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.png", "start": 27597330, "end": 28703488, "audio": 0}, {"filename": "/resources/img/btnProp-Blue.png", "start": 28703488, "end": 28705484, "audio": 0}, {"filename": "/resources/img/o303.png", "start": 28705484, "end": 29424679, "audio": 0}, {"filename": "/resources/img/btnPatD.png", "start": 29424679, "end": 29432203, "audio": 0}, {"filename": "/resources/img/btnPatB.png", "start": 29432203, "end": 29439760, "audio": 0}], "remote_package_size": 29439760, "package_uuid": "4183ad46-ceab-4eb6-b9c6-99e38e6d96ea"});
  
  })();
  
