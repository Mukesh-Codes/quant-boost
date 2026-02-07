import yfinance as yf
from datetime import date, timedelta
import os

TICKERS = [
    "AAPL", "MSFT", "GOOG", "AMZN", "META",
    "NVDA", "TSLA", "JPM", "XOM", "UNH"
]

END = date.today()
START = END - timedelta(days=365)

os.makedirs("data", exist_ok=True)

for ticker in TICKERS:
    print(f"Downloading {ticker}")
    df = yf.download(ticker, start=START, end=END, progress=False)
    df.columns = df.columns.get_level_values(0)
    df.reset_index(inplace=True)
    df.to_csv(f"data/{ticker}.csv")

print("Done.")
