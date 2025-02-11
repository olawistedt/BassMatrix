
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
   loadPackage({"files": [{"filename": "/resources/img/btnPtnCopy.png", "start": 0, "end": 11095, "audio": 0}, {"filename": "/resources/img/btnWaveForm.png", "start": 11095, "end": 17944, "audio": 0}, {"filename": "/resources/img/606-2-ola-1.png", "start": 17944, "end": 30838, "audio": 0}, {"filename": "/resources/img/btnPatB.png", "start": 30838, "end": 38395, "audio": 0}, {"filename": "/resources/img/btnPatC.png", "start": 38395, "end": 45994, "audio": 0}, {"filename": "/resources/img/btnC.knob", "start": 45994, "end": 2427116, "audio": 0}, {"filename": "/resources/img/knobLoopSize.knob", "start": 2427116, "end": 2519614, "audio": 0}, {"filename": "/resources/img/btnPatA.png", "start": 2519614, "end": 2527215, "audio": 0}, {"filename": "/resources/img/btnSeq.knob", "start": 2527215, "end": 4913622, "audio": 0}, {"filename": "/resources/img/btnPatE.png", "start": 4913622, "end": 4919261, "audio": 0}, {"filename": "/resources/img/o303-1130x840.png", "start": 4919261, "end": 5595823, "audio": 0}, {"filename": "/resources/img/btnPtnClear.png", "start": 5595823, "end": 5607604, "audio": 0}, {"filename": "/resources/img/btnPatC#.png", "start": 5607604, "end": 5615467, "audio": 0}, {"filename": "/resources/img/btnLed.knob", "start": 5615467, "end": 5656590, "audio": 0}, {"filename": "/resources/img/btnPatF.png", "start": 5656590, "end": 5664021, "audio": 0}, {"filename": "/resources/img/knobLoopSize.png", "start": 5664021, "end": 5784883, "audio": 0}, {"filename": "/resources/img/btnPatF#.png", "start": 5784883, "end": 5792613, "audio": 0}, {"filename": "/resources/img/606-2.knob", "start": 5792613, "end": 5881081, "audio": 0}, {"filename": "/resources/img/btnHostSync.png", "start": 5881081, "end": 5893944, "audio": 0}, {"filename": "/resources/img/btnPatA#.png", "start": 5893944, "end": 5901811, "audio": 0}, {"filename": "/resources/img/btnKeySync.png", "start": 5901811, "end": 5914496, "audio": 0}, {"filename": "/resources/img/background-1130x840.xcf", "start": 5914496, "end": 6964111, "audio": 0}, {"filename": "/resources/img/o303.png", "start": 6964111, "end": 7683306, "audio": 0}, {"filename": "/resources/img/btnEffects.xcf", "start": 7683306, "end": 7695408, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.knob", "start": 7695408, "end": 7793830, "audio": 0}, {"filename": "/resources/img/btnSeq.png", "start": 7793830, "end": 7795863, "audio": 0}, {"filename": "/resources/img/btnAccent.knob", "start": 7795863, "end": 7836739, "audio": 0}, {"filename": "/resources/img/btnHostSync.knob", "start": 7836739, "end": 10211894, "audio": 0}, {"filename": "/resources/img/btnLed.png", "start": 10211894, "end": 10216573, "audio": 0}, {"filename": "/resources/img/btnC.png", "start": 10216573, "end": 10219478, "audio": 0}, {"filename": "/resources/img/btnPatternOctav.knob", "start": 10219478, "end": 12601131, "audio": 0}, {"filename": "/resources/img/fx-1-100x100.png", "start": 12601131, "end": 13597726, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.png", "start": 13597726, "end": 14703884, "audio": 0}, {"filename": "/resources/img/606-2.png", "start": 14703884, "end": 15956515, "audio": 0}, {"filename": "/resources/img/btnProp-Blue.png", "start": 15956515, "end": 15958511, "audio": 0}, {"filename": "/resources/img/btnPatG.png", "start": 15958511, "end": 15966138, "audio": 0}, {"filename": "/resources/img/fx1-1.png", "start": 15966138, "end": 18279788, "audio": 0}, {"filename": "/resources/img/btnPatD.png", "start": 18279788, "end": 18287312, "audio": 0}, {"filename": "/resources/img/btnStop.png", "start": 18287312, "end": 18298318, "audio": 0}, {"filename": "/resources/img/btnPatMod.knob", "start": 18298318, "end": 20674001, "audio": 0}, {"filename": "/resources/img/btnPtnRand.png", "start": 20674001, "end": 20686535, "audio": 0}, {"filename": "/resources/img/background-1130x840.png", "start": 20686535, "end": 21090559, "audio": 0}, {"filename": "/resources/img/btnAccent.bmp", "start": 21090559, "end": 21115189, "audio": 0}, {"filename": "/resources/img/btnPatternOctav2.png", "start": 21115189, "end": 21127671, "audio": 0}, {"filename": "/resources/img/btnProp.png", "start": 21127671, "end": 21129658, "audio": 0}, {"filename": "/resources/img/btnMidi.png", "start": 21129658, "end": 21142105, "audio": 0}, {"filename": "/resources/img/btnEffects.png", "start": 21142105, "end": 21150192, "audio": 0}, {"filename": "/resources/img/btnPatG#.png", "start": 21150192, "end": 21158074, "audio": 0}, {"filename": "/resources/img/btnPatD#.png", "start": 21158074, "end": 21165861, "audio": 0}, {"filename": "/resources/img/btnInternalSync.png", "start": 21165861, "end": 21178318, "audio": 0}, {"filename": "/resources/img/btnProp-Turkos.png", "start": 21178318, "end": 21180305, "audio": 0}, {"filename": "/resources/img/btnProp-Brons.png", "start": 21180305, "end": 21182377, "audio": 0}, {"filename": "/resources/img/btnPatternOctav3.png", "start": 21182377, "end": 21195152, "audio": 0}, {"filename": "/resources/img/ola-background-1130x840.xcf", "start": 21195152, "end": 29452745, "audio": 0}], "remote_package_size": 29452745, "package_uuid": "c6f5267e-df65-4c7e-b258-cfee775cad39"});
  
  })();
  
