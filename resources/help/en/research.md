# Research

Research studies one company at a time. The symbol picker under the header takes a ticker or a company name (any Yahoo Finance symbol works: `AAPL`, `MC.PA`, `7203.T`), with the name, price and day's change next to it; a sub-header lists the sections. The symbol and the section are remembered. Everything is downloaded on demand and kept on disk, from 15 minutes for quotes to 30 days for head counts; the groups of hosts are listed in [Sources](help:sources) under Research.

## Sections

- **Key metrics**: the profile (sector, industry, country, CEO, description, website, employees), the indexes it belongs to, head count over the years (Wikidata), market cap against the trailing year's revenue and profit, and fifteen cards of ratios: valuation, earnings, per share, balance sheet, margins, profitability, leverage, dividends, ownership, growth (3- and 5-year), efficiency and liquidity. Balance figures come from the latest quarterly balance sheet; ratios of yearly flows to balances take their figures from one fiscal year (the tooltip names it). Statements in another currency than the quote are converted.
- **Financials**: the income statement, balance sheet and cash flow as reported, annual (four years) or quarterly (five quarters).
- **Financial charts**: revenue, gross profit, operating and net income, margins, EPS, cash flow and balance sheet, yearly or quarterly over 5, 10, 15 years or everything, with the change on each bar. US filers come from their SEC filings since about 2009 (the fourth quarter is the year less the first nine months); others from Yahoo's four years. Per-share figures are adjusted for the splits after each filing.
- **Ownership**: institutions, insiders and the public, the 25 largest institutional holders with their change (from 13F filings), insiders' open-market sales and purchases per quarter and their last 50 trades.
- **Ratings**: this month's analyst counts and their gauge, the mean recommendation, price targets over a year of monthly closes, every firm's rating changes and the recommendation history rebuilt from them. The **intrinsic value**, **factor grades** and **quant rating** are Deepwater's own and tagged **EXPERIMENTAL**: a two-stage discounted cash flow with fixed assumptions (explained in the tooltip; a point of discount rate moves a large company's value by about 17 %), letter grades on fixed thresholds, a 1–5 rating weighing valuation, profitability, financial health and momentum. They haven't been validated against later prices.
- **Compare**: peers, the company's sector or the whole market, filtered by sector and industry, on six sets of columns. Click a row to open that company.
- **Compare charts**: one reported figure of two companies, quarterly or yearly, as values or growth against the same period a year before.
- **Sectors**: the S&P 500, the Nasdaq-100 and the Dow by sector, today, over 1 week, 1 or 3 months or since the start of the year: a ring weighted by market cap, coloured by change, and the sectors with their members. The official index's change is shown next to the recomputed one as a check.
- **ICS**: every US listing in Nasdaq's sector › industry classification (about 7,000 stocks), with totals and each industry's members. The search box matches industries and companies; a double click opens a company.
- **SPLC**: the supply chain graph: suppliers, the company, customers, from Wikidata through the products they make for each other. Each link counts products, not revenue; coverage is uneven (Apple's suppliers are well documented, Tesla's hardly), since Wikidata is edited by volunteers. Click a company to open it.

## Sources

Yahoo Finance (quotes, statements, holders, analysts, related symbols), Nasdaq (listings, holders, insider trades, screener), the SEC (XBRL company facts), Wikipedia (index members) and Wikidata (head counts, supply chains). Figures and names that come from the data stay as published; the country and the sector names are translated.
