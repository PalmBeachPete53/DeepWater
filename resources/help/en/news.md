# News

A wire of headlines in five tabs: **General**, **Stocks**, **Crypto**, **Forex** and your **Favorites**, with a ticker filter ("NVDA", "BTCUSD", "EURUSD"). It is refreshed every five minutes while shown, and on **Refresh**. Two icons next to Refresh choose the **grid** (as many columns as fit) or the **column** view (one compact card per line); the choice is kept.

## Cards and the reader

Each card is a headline, its summary, the date and how long ago, the article's site and the ticker as a badge. A click opens the article in the **reader**, a floating panel over the page: the web page itself, the title, site, date and ticker, a back button for links followed inside, **Open in the browser**, and × or Escape or a click outside to close. A middle click or ⌘-click on a card opens the browser directly.

The star in a card's corner, or in the reader's header, keeps the article in **Favorites**, as it was when starred.

The reader runs an **ad blocker**, Brave's engine with the lists it turns on by default (uBlock Origin's, EasyList, EasyPrivacy, cookie notices…), downloaded once a day when the tab is first opened. The shield in the reader's header shows its state. The lists can be refreshed or the blocker turned off in [Sources](help:sources) › News. On macOS the reader renders with the system's WebKit (Safari's engine), which some sites' bot protections accept where other engines are refused; when a site still refuses (401, 403, 429), the reader says so under its header and **Open in the browser** is the way.

## Sources

Without a key, free RSS feeds merged and sorted newest first: CNBC, MarketWatch and BBC Business (general); Nasdaq, MarketWatch, Seeking Alpha and Investing.com (stocks); Cointelegraph, Decrypt, CoinDesk and Investing.com (crypto); FXStreet and Investing.com (forex). The ticker badge comes from the feed when it gives one, else from the text ("(NASDAQ: NFLX)", "$NVDA", the coin or currency named first). With the ticker filter, the Stocks tab reads Yahoo Finance's and Nasdaq's feeds about that symbol; the other tabs keep the articles that name it.

With a **Financial Modeling Prep** key saved in Sources › News, its news endpoints give every article a ticker and the filter asks them directly. A failed request falls back to the feeds and says why in the strip.
