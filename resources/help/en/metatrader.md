# MetaTrader

One order for several MetaTrader accounts, sized for each by its own balance, from Deepwater. **These are real orders.** No AI can read this page or send anything from it.

The accounts are reached through **MetaCopier**, a paid cloud service that keeps the connections and holds your MetaTrader logins; Deepwater only holds its API key, in the system's secret store (Keychain on macOS, Credential Manager on Windows). Check that your broker or prop firm allows this before using it. FTMO limits capital to $400,000 per trader or strategy and may suspend accounts with identical trades over it; whether it accepts a service holding your logins is a question for FTMO.

## Setting up

1. Create a MetaCopier account, add your MetaTrader accounts there, and make an API key.
2. Paste the key here and **Save**: it is tried first, and a refused key isn't kept.
3. **Start with a demo account.**

## Sending an order

1. Tick **Armed** on the accounts that should take it. Nothing is armed when the app starts.
2. Fill in the symbol, the side, the risk per account in percent of its balance (5 % at most) and a stop, which is required. The target is optional.
3. **Prepare** shows, per account, the symbol under its name there, the price, the lots and what they risk, or why that account refuses. Lots are rounded down, so an account never risks more than asked; too small for the minimum volume, it refuses rather than rounding up.
4. **Send** asks for confirmation, then sends once to each account that is ready. A plan lasts 30 seconds and is used up by sending.
5. Each line then says what happened: **Open** when the position was seen on the account, **Refused** with the reason, or **Not seen on the account**. In that last case look in MetaTrader before sending again: **Deepwater never resends an order by itself**, since one whose answer got lost may have been executed.

## Real money

An account the service doesn't report as a demo, or doesn't report at all, counts as real money and can't be armed until you allow it, by typing a word, for this session only. A prop firm's challenge may be reported as a demo: arming it is still your decision, and the server name is shown to help you tell.

## Positions

The positions of every account, refreshed every few seconds while the page is open. **Close** ends one, after a question. **Close all on the armed accounts** ends every position on them, including those not opened from Deepwater.

Balances come from MetaCopier, which syncs them once a minute. A slow answer or a symbol's rules can make the fill differ from the plan's price.
