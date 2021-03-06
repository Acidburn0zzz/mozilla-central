/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.io.IOException;
import java.net.URISyntaxException;
import java.util.concurrent.ExecutorService;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.CredentialsSource;
import org.mozilla.gecko.sync.EngineSettings;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.MetaGlobalException;
import org.mozilla.gecko.sync.NoCollectionKeysSetException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SynchronizerConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.WipeServerDelegate;
import org.mozilla.gecko.sync.middleware.Crypto5MiddlewareRepository;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncStorageRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.Server11Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.synchronizer.Synchronizer;
import org.mozilla.gecko.sync.synchronizer.SynchronizerDelegate;

import android.content.Context;

/**
 * Fetch from a server collection into a local repository, encrypting
 * and decrypting along the way.
 *
 * @author rnewman
 *
 */
public abstract class ServerSyncStage implements
    GlobalSyncStage,
    SynchronizerDelegate {

  protected static final String LOG_TAG = "ServerSyncStage";

  protected final GlobalSession session;

  public ServerSyncStage(GlobalSession session) {
    if (session == null) {
      throw new IllegalArgumentException("session must not be null.");
    }
    this.session = session;
  }

  /**
   * Override these in your subclasses.
   *
   * @return true if this stage should be executed.
   * @throws MetaGlobalException
   */
  protected boolean isEnabled() throws MetaGlobalException {
    EngineSettings engineSettings = null;
    try {
       engineSettings = getEngineSettings();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Unable to get engine settings for " + this + ": fetching config failed.", e);
      // Fall through; null engineSettings will pass below.
    }

    // We catch the subclasses of MetaGlobalException to trigger various resets and wipes in execute().
    return session.engineIsEnabled(this.getEngineName(), engineSettings);
  }

  protected EngineSettings getEngineSettings() throws NonObjectJSONException, IOException, ParseException {
    Integer version = getStorageVersion();
    if (version == null) {
      Logger.warn(LOG_TAG, "null storage version for " + this + "; using version 0.");
      version = new Integer(0);
    }

    SynchronizerConfiguration config = this.getConfig();
    if (config == null) {
      return new EngineSettings(null, version.intValue());
    }
    return new EngineSettings(config.syncID, version.intValue());
  }

  protected abstract String getCollection();
  protected abstract String getEngineName();
  protected abstract Repository getLocalRepository();
  protected abstract RecordFactory getRecordFactory();

  // Override this in subclasses.
  protected Repository getRemoteRepository() throws URISyntaxException {
    return new Server11Repository(session.config.getClusterURLString(),
                                  session.config.username,
                                  getCollection(),
                                  session);
  }

  /**
   * Return a Crypto5Middleware-wrapped Server11Repository.
   *
   * @throws NoCollectionKeysSetException
   * @throws URISyntaxException
   */
  protected Repository wrappedServerRepo() throws NoCollectionKeysSetException, URISyntaxException {
    String collection = this.getCollection();
    KeyBundle collectionKey = session.keyBundleForCollection(collection);
    Crypto5MiddlewareRepository cryptoRepo = new Crypto5MiddlewareRepository(getRemoteRepository(), collectionKey);
    cryptoRepo.recordFactory = getRecordFactory();
    return cryptoRepo;
  }

  protected String bundlePrefix() {
    return this.getCollection() + ".";
  }

  protected SynchronizerConfiguration getConfig() throws NonObjectJSONException, IOException, ParseException {
    return new SynchronizerConfiguration(session.config.getBranch(bundlePrefix()));
  }

  protected void persistConfig(SynchronizerConfiguration synchronizerConfiguration) {
    synchronizerConfiguration.persist(session.config.getBranch(bundlePrefix()));
  }

  public Synchronizer getConfiguredSynchronizer(GlobalSession session) throws NoCollectionKeysSetException, URISyntaxException, NonObjectJSONException, IOException, ParseException {
    Repository remote = wrappedServerRepo();

    Synchronizer synchronizer = new Synchronizer();
    synchronizer.repositoryA = remote;
    synchronizer.repositoryB = this.getLocalRepository();
    synchronizer.load(getConfig());

    return synchronizer;
  }

  /**
   * Reset timestamps.
   */
  @Override
  public void resetLocal() {
    resetLocal(null);
  }

  /**
   * Reset timestamps and possibly set syncID.
   * @param syncID if non-null, new syncID to persist.
   */
  public void resetLocal(String syncID) {
    // Clear both timestamps.
    SynchronizerConfiguration config;
    try {
      config = this.getConfig();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Unable to reset " + this + ": fetching config failed.", e);
      return;
    }

    if (syncID != null) {
      config.syncID = syncID;
      Logger.info(LOG_TAG, "Setting syncID for " + this + " to '" + syncID + "'.");
    }
    config.localBundle.setTimestamp(0L);
    config.remoteBundle.setTimestamp(0L);
    persistConfig(config);
    Logger.info(LOG_TAG, "Reset timestamps for " + this);
  }

  // Not thread-safe. Use with caution.
  private class WipeWaiter {
    public boolean sessionSucceeded = true;
    public boolean wipeSucceeded = true;
    public Exception error;

    public void notify(Exception e, boolean sessionSucceeded) {
      this.sessionSucceeded = sessionSucceeded;
      this.wipeSucceeded = false;
      this.error = e;
      this.notify();
    }
  }

  /**
   * Synchronously wipe this stage by instantiating a local repository session
   * and wiping that.
   * <p>
   * Logs and re-throws an exception on failure.
   */
  @Override
  public void wipeLocal() throws Exception {
    // Reset, then clear data.
    this.resetLocal();

    final WipeWaiter monitor = new WipeWaiter();
    final Context context = session.getContext();
    final Repository r = this.getLocalRepository();

    final Runnable doWipe = new Runnable() {
      @Override
      public void run() {
        r.createSession(new RepositorySessionCreationDelegate() {

          @Override
          public void onSessionCreated(final RepositorySession session) {
            try {
              session.begin(new RepositorySessionBeginDelegate() {

                @Override
                public void onBeginSucceeded(final RepositorySession session) {
                  session.wipe(new RepositorySessionWipeDelegate() {
                    @Override
                    public void onWipeSucceeded() {
                      try {
                        session.finish(new RepositorySessionFinishDelegate() {

                          @Override
                          public void onFinishSucceeded(RepositorySession session,
                                                        RepositorySessionBundle bundle) {
                            // Hurrah.
                            synchronized (monitor) {
                              monitor.notify();
                            }
                          }

                          @Override
                          public void onFinishFailed(Exception ex) {
                            // Assume that no finish => no wipe.
                            synchronized (monitor) {
                              monitor.notify(ex, true);
                            }
                          }

                          @Override
                          public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService executor) {
                            return this;
                          }
                        });
                      } catch (InactiveSessionException e) {
                        // Cannot happen. Call for safety.
                        synchronized (monitor) {
                          monitor.notify(e, true);
                        }
                      }
                    }

                    @Override
                    public void onWipeFailed(Exception ex) {
                      session.abort();
                      synchronized (monitor) {
                        monitor.notify(ex, true);
                      }
                    }

                    @Override
                    public RepositorySessionWipeDelegate deferredWipeDelegate(ExecutorService executor) {
                      return this;
                    }
                  });
                }

                @Override
                public void onBeginFailed(Exception ex) {
                  session.abort();
                  synchronized (monitor) {
                    monitor.notify(ex, true);
                  }
                }

                @Override
                public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService executor) {
                  return this;
                }
              });
            } catch (InvalidSessionTransitionException e) {
              session.abort();
              synchronized (monitor) {
                monitor.notify(e, true);
              }
            }
          }

          @Override
          public void onSessionCreateFailed(Exception ex) {
            synchronized (monitor) {
              monitor.notify(ex, false);
            }
          }

          @Override
          public RepositorySessionCreationDelegate deferredCreationDelegate() {
            return this;
          }
        }, context);
      }
    };

    final Thread wiping = new Thread(doWipe);
    synchronized (monitor) {
      wiping.start();
      try {
        monitor.wait();
      } catch (InterruptedException e) {
        Logger.error(LOG_TAG, "Wipe interrupted.");
      }
    }

    if (!monitor.sessionSucceeded) {
      Logger.error(LOG_TAG, "Failed to create session for wipe.");
      throw monitor.error;
    }

    if (!monitor.wipeSucceeded) {
      Logger.error(LOG_TAG, "Failed to wipe session.");
      throw monitor.error;
    }

    Logger.info(LOG_TAG, "Wiping stage complete.");
  }

  /**
   * Asynchronously wipe collection on server.
   */
  protected void wipeServer(final CredentialsSource credentials, final WipeServerDelegate wipeDelegate) {
    SyncStorageRequest request;

    try {
      request = new SyncStorageRequest(session.config.collectionURI(getCollection()));
    } catch (URISyntaxException ex) {
      Logger.warn(LOG_TAG, "Invalid URI in wipeServer.");
      wipeDelegate.onWipeFailed(ex);
      return;
    }

    request.delegate = new SyncStorageRequestDelegate() {

      @Override
      public String ifUnmodifiedSince() {
        return null;
      }

      @Override
      public void handleRequestSuccess(SyncStorageResponse response) {
        BaseResource.consumeEntity(response);
        resetLocal();
        wipeDelegate.onWiped(response.normalizedWeaveTimestamp());
      }

      @Override
      public void handleRequestFailure(SyncStorageResponse response) {
        Logger.warn(LOG_TAG, "Got request failure " + response.getStatusCode() + " in wipeServer.");
        // Process HTTP failures here to pick up backoffs, etc.
        session.interpretHTTPFailure(response.httpResponse());
        BaseResource.consumeEntity(response); // The exception thrown should not need the body of the response.
        wipeDelegate.onWipeFailed(new HTTPFailureException(response));
      }

      @Override
      public void handleRequestError(Exception ex) {
        Logger.warn(LOG_TAG, "Got exception in wipeServer.", ex);
        wipeDelegate.onWipeFailed(ex);
      }

      @Override
      public String credentials() {
        return credentials.credentials();
      }
    };

    request.delete();
  }

  /**
   * Synchronously wipe the server.
   * <p>
   * Logs and re-throws an exception on failure.
   */
  public void wipeServer() throws Exception {
    final WipeWaiter monitor = new WipeWaiter();

    final Runnable doWipe = new Runnable() {
      @Override
      public void run() {
        wipeServer(session, new WipeServerDelegate() {
          @Override
          public void onWiped(long timestamp) {
            synchronized (monitor) {
              monitor.notify();
            }
          }

          @Override
          public void onWipeFailed(Exception e) {
            synchronized (monitor) {
              monitor.notify(e, false);
            }
          }
        });
      }
    };

    final Thread wiping = new Thread(doWipe);
    synchronized (monitor) {
      wiping.start();
      try {
        monitor.wait();
      } catch (InterruptedException e) {
        Logger.error(LOG_TAG, "Server wipe interrupted.");
      }
    }

    if (!monitor.wipeSucceeded) {
      Logger.error(LOG_TAG, "Failed to wipe server.");
      throw monitor.error;
    }

    Logger.info(LOG_TAG, "Wiping server complete.");
  }

  @Override
  public void execute() throws NoSuchStageException {
    final String name = getEngineName();
    Logger.debug(LOG_TAG, "Starting execute for " + name);

    try {
      if (!this.isEnabled()) {
        Logger.info(LOG_TAG, "Stage " + name + " disabled; skipping.");
        session.advance();
        return;
      }
    } catch (MetaGlobalException.MetaGlobalMalformedSyncIDException e) {
      // Bad engine syncID. This should never happen. Wipe the server.
      try {
        session.updateMetaGlobalWith(name, new EngineSettings(Utils.generateGuid(), this.getStorageVersion()));
        Logger.info(LOG_TAG, "Wiping server because malformed engine sync ID was found in meta/global.");
        wipeServer();
        Logger.info(LOG_TAG, "Wiped server after malformed engine sync ID found in meta/global.");
      } catch (Exception ex) {
        session.abort(ex, "Failed to wipe server after malformed engine sync ID found in meta/global.");
      }
    } catch (MetaGlobalException.MetaGlobalMalformedVersionException e) {
      // Bad engine version. This should never happen. Wipe the server.
      try {
        session.updateMetaGlobalWith(name, new EngineSettings(Utils.generateGuid(), this.getStorageVersion()));
        Logger.info(LOG_TAG, "Wiping server because malformed engine version was found in meta/global.");
        wipeServer();
        Logger.info(LOG_TAG, "Wiped server after malformed engine version found in meta/global.");
      } catch (Exception ex) {
        session.abort(ex, "Failed to wipe server after malformed engine version found in meta/global.");
      }
    } catch (MetaGlobalException.MetaGlobalStaleClientSyncIDException e) {
      // Our syncID is wrong. Reset client and take the server syncID.
      Logger.warn(LOG_TAG, "Remote engine syncID different from local engine syncID:" +
                           " resetting local engine and assuming remote engine syncID.");
      this.resetLocal(e.serverSyncID);
    } catch (MetaGlobalException e) {
      session.abort(e, "Inappropriate meta/global; refusing to execute " + name + " stage.");
      return;
    }

    Synchronizer synchronizer;
    try {
      synchronizer = this.getConfiguredSynchronizer(session);
    } catch (NoCollectionKeysSetException e) {
      session.abort(e, "No CollectionKeys.");
      return;
    } catch (URISyntaxException e) {
      session.abort(e, "Invalid URI syntax for server repository.");
      return;
    } catch (NonObjectJSONException e) {
      session.abort(e, "Invalid persisted JSON for config.");
      return;
    } catch (IOException e) {
      session.abort(e, "Invalid persisted JSON for config.");
      return;
    } catch (ParseException e) {
      session.abort(e, "Invalid persisted JSON for config.");
      return;
    }

    Logger.debug(LOG_TAG, "Invoking synchronizer.");
    synchronizer.synchronize(session.getContext(), this);
    Logger.debug(LOG_TAG, "Reached end of execute.");
  }

  @Override
  public void onSynchronized(Synchronizer synchronizer) {
    Logger.debug(LOG_TAG, "onSynchronized.");

    SynchronizerConfiguration newConfig = synchronizer.save();
    if (newConfig != null) {
      persistConfig(newConfig);
    } else {
      Logger.warn(LOG_TAG, "Didn't get configuration from synchronizer after success.");
    }

    Logger.info(LOG_TAG, "Advancing session.");
    session.advance();
  }

  @Override
  public void onSynchronizeFailed(Synchronizer synchronizer,
                                  Exception lastException, String reason) {
    Logger.debug(LOG_TAG, "onSynchronizeFailed: " + reason);

    // This failure could be due to a 503 or a 401 and it could have headers.
    if (lastException instanceof HTTPFailureException) {
      session.handleHTTPError(((HTTPFailureException)lastException).response, reason);
    } else {
      session.abort(lastException, reason);
    }
  }

  @Override
  public void onSynchronizeAborted(Synchronizer synchronize) {
    Logger.info(LOG_TAG, "onSynchronizeAborted.");

    session.abort(null, "Synchronization was aborted.");
  }
}
