Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

Engines.register(BookmarksEngine);
let engine = Engines.get("bookmarks");
let store  = engine._store;
store.wipe();

function test_tracking() {
  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  let folder = Svc.Bookmark.createFolder(Svc.Bookmark.bookmarksMenuFolder,
                                         "Test Folder",
                                         Svc.Bookmark.DEFAULT_INDEX);
  function createBmk() {
    return Svc.Bookmark.insertBookmark(folder,
                                       Utils.makeURI("http://getfirefox.com"),
                                       Svc.Bookmark.DEFAULT_INDEX,
                                       "Get Firefox!");
  }

  try {
    _("Create bookmark. Won't show because we haven't started tracking yet");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createBmk();
    // We expect two changed items because the containing folder
    // changed as well (new child).
    do_check_eq([id for (id in tracker.changedIDs)].length, 2);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 3);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    createBmk();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
}

function test_onItemChanged() {
  // Anno that's in ANNOS_TO_TRACK.
  const DESCRIPTION_ANNO = "bookmarkProperties/description";

  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  try {
    Svc.Obs.notify("weave:engine:stop-tracking");
    let folder = Svc.Bookmark.createFolder(Svc.Bookmark.bookmarksMenuFolder,
                                           "Parent",
                                           Svc.Bookmark.DEFAULT_INDEX);
    _("Track changes to annos.");
    let b = Svc.Bookmark.insertBookmark(folder,
                                        Utils.makeURI("http://getfirefox.com"),
                                        Svc.Bookmark.DEFAULT_INDEX,
                                        "Get Firefox!");
    let bGUID = engine._store.GUIDForId(b);
    _("New item is " + b);
    _("GUID: " + bGUID);

    Svc.Obs.notify("weave:engine:start-tracking");
    Svc.Annos.setItemAnnotation(b, DESCRIPTION_ANNO, "A test description", 0,
                                Svc.Annos.EXPIRE_NEVER);
    do_check_true(tracker.changedIDs[bGUID] > 0);

  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
}

function test_onItemMoved() {
  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  try {
    let fx_id = Svc.Bookmark.insertBookmark(
      Svc.Bookmark.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      Svc.Bookmark.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    let tb_id = Svc.Bookmark.insertBookmark(
      Svc.Bookmark.bookmarksMenuFolder,
      Utils.makeURI("http://getthunderbird.com"),
      Svc.Bookmark.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = engine._store.GUIDForId(tb_id);

    Svc.Obs.notify("weave:engine:start-tracking");

    // Moving within the folder will just track the folder.
    Svc.Bookmark.moveItem(tb_id, Svc.Bookmark.bookmarksMenuFolder, 0);
    do_check_true(tracker.changedIDs['menu'] > 0);
    do_check_eq(tracker.changedIDs['toolbar'], undefined);
    do_check_eq(tracker.changedIDs[fx_guid], undefined);
    do_check_eq(tracker.changedIDs[tb_guid], undefined);
    tracker.clearChangedIDs();

    // Moving a bookmark to a different folder will track the old
    // folder, the new folder and the bookmark.
    Svc.Bookmark.moveItem(tb_id, Svc.Bookmark.toolbarFolder,
                          Svc.Bookmark.DEFAULT_INDEX);
    do_check_true(tracker.changedIDs['menu'] > 0);
    do_check_true(tracker.changedIDs['toolbar'] > 0);
    do_check_eq(tracker.changedIDs[fx_guid], undefined);
    do_check_true(tracker.changedIDs[tb_guid] > 0);

  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }

}

function run_test() {
  initTestLogging("Trace");

  Log4Moz.repository.getLogger("Engine.Bookmarks").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Store.Bookmarks").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Tracker.Bookmarks").level = Log4Moz.Level.Trace;

  test_tracking();
  test_onItemChanged();
  test_onItemMoved();
}

