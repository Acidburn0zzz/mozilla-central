/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let pb = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);

  // utility functions
  function countClosedTabsByTitle(aClosedTabList, aTitle)
    aClosedTabList.filter(function (aData) aData.title == aTitle).length;

  function countOpenTabsByTitle(aOpenTabList, aTitle)
    aOpenTabList.filter(function (aData) aData.entries.some(function (aEntry) aEntry.title == aTitle)).length

  // backup old state
  let oldState = ss.getBrowserState();
  let oldState_wins = JSON.parse(oldState).windows.length;
  if (oldState_wins != 1)
    ok(false, "oldState in test_purge has " + oldState_wins + " windows instead of 1");

  // create a new state for testing
  const REMEMBER = Date.now(), FORGET = Math.random();
  let testState = {
    windows: [ { tabs: [{ entries: [{ url: "http://example.com/" }] }], selected: 1 } ],
    _closedWindows : [
      // _closedWindows[0]
      {
        tabs: [
          { entries: [{ url: "http://example.com/", title: REMEMBER }] },
          { entries: [{ url: "http://mozilla.org/", title: FORGET }] }
        ],
        selected: 2,
        title: "mozilla.org",
        _closedTabs: []
      },
      // _closedWindows[1]
      {
        tabs: [
         { entries: [{ url: "http://mozilla.org/", title: FORGET }] },
         { entries: [{ url: "http://example.com/", title: REMEMBER }] },
         { entries: [{ url: "http://example.com/", title: REMEMBER }] },
         { entries: [{ url: "http://mozilla.org/", title: FORGET }] },
         { entries: [{ url: "http://example.com/", title: REMEMBER }] }
        ],
        selected: 5,
        _closedTabs: []
      },
      // _closedWindows[2]
      {
        tabs: [
          { entries: [{ url: "http://example.com/", title: REMEMBER }] }
        ],
        selected: 1,
        _closedTabs: [
          {
            state: {
              entries: [
                { url: "http://mozilla.org/", title: FORGET },
                { url: "http://mozilla.org/again", title: "doesn't matter" }
              ]
            },
            pos: 1,
            title: FORGET
          },
          {
            state: {
              entries: [
                { url: "http://example.com", title: REMEMBER }
              ]
            },
            title: REMEMBER
          }
        ]
      }
    ]
  };
  
  // set browser to test state
  ss.setBrowserState(JSON.stringify(testState));

  // purge domain & check that we purged correctly for closed windows
  pb.removeDataFromDomain("mozilla.org");

  let closedWindowData = JSON.parse(ss.getClosedWindowData());

  // First set of tests for _closedWindows[0] - tests basics
  let win = closedWindowData[0];
  is(win.tabs.length, 1, "1 tab was removed");
  is(countOpenTabsByTitle(win.tabs, FORGET), 0,
     "The correct tab was removed");
  is(countOpenTabsByTitle(win.tabs, REMEMBER), 1,
     "The correct tab was remembered");
  is(win.selected, 1, "Selected tab has changed");
  is(win.title, REMEMBER, "The window title was correctly updated");

  // Test more complicated case 
  win = closedWindowData[1];
  is(win.tabs.length, 3, "2 tabs were removed");
  is(countOpenTabsByTitle(win.tabs, FORGET), 0,
     "The correct tabs were removed");
  is(countOpenTabsByTitle(win.tabs, REMEMBER), 3,
     "The correct tabs were remembered");
  is(win.selected, 3, "Selected tab has changed");
  is(win.title, REMEMBER, "The window title was correctly updated");

  // Tests handling of _closedTabs
  win = closedWindowData[2];
  is(countClosedTabsByTitle(win._closedTabs, REMEMBER), 1,
     "The correct number of tabs were removed, and the correct ones");
  is(countClosedTabsByTitle(win._closedTabs, FORGET), 0,
     "All tabs to be forgotten were indeed removed");

  // restore pre-test state
  ss.setBrowserState(oldState);
}
