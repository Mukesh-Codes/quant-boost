#include "ThreadPool.h"
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <xgboost/c_api.h>


namespace fs = std::filesystem;
using namespace std;

struct StockData {
    string symbol;
    vector<string> dates;
    vector<double> close, high, low, open;
    vector<long long> volume;
    vector<double> returns;
    vector<double> sma_20;
    vector<double> rsi_14;
    vector<double> momentum_1m;
    vector<double> momentum_3m;
    vector<double> momentum_6m;
    vector<double> volatility_20;
};

struct Dataset{
    vector<vector<float>> X;
    vector<float> Y;
    vector<vector<float>> X_train;
    vector<vector<float>> Y_train;
    vector<vector<float>> X_test;
    vector<vector<float>> Y_test;
};

Dataset dataset;

void buildDataset(vector<StockData>& stock_data, Dataset& dataset) {
    for (const StockData& stock: stock_data){
        vector<float> row;
        for(int t=0; t<data.close.size() - 1; t++){
            row.push_back(stock.momentum_1m[t]);
            row.push_back(stock.momentum_3m[t]);
            row.push_back(stock.momentum_6m[t]);
            row.push_back(stock.volatility_20[t]);
            row.push_back(stock.rsi_14[t]);
            row.push_back(stock.sma_20[t]);
            dataset.Y.push_back(stock.returns[t+1]);
        }
        dataset.push_back(row);
    }
}

bool loadStockData(const std::string& filepath, StockData& data) {
    ifstream file(filepath);
    if (!file.is_open()) {
        cerr << "Failed to open " << filepath << endl;
        return false;
    }
    string line;
    getline(file, line);
    while (getline(file, line)) {
        stringstream ss(line);
        string token;
        getline(ss, token, ',');
        getline(ss, token, ',');
        data.dates.push_back(token);
        getline(ss, token, ',');
        data.open.push_back(stod(token));
        getline(ss, token, ',');
        data.high.push_back(stod(token));
        getline(ss, token, ',');
        data.low.push_back(stod(token));
        getline(ss, token, ',');
        data.close.push_back(stod(token));
        getline(ss, token, ',');
        data.volume.push_back(stoll(token));
    }

    return true;
}

void calculateReturns(StockData& data) {
    data.returns.resize(data.close.size());
    for (size_t i = 1; i < data.close.size(); ++i) {
        data.returns[i] = (data.close[i] - data.close[i-1]) / data.close[i-1];
    }
    data.returns[0] = 0.0;}

void calculateSMA(StockData& data, int period = 20) {
    data.sma_20.resize(data.close.size());
    for (size_t i = 0; i < data.close.size(); ++i) {
        if (i < period - 1) {
            data.sma_20[i] = 0.0;
        } else {
            double sum = 0.0;
            for (int j = 0; j < period; ++j) {
                sum += data.close[i - j];
            }
            data.sma_20[i] = sum /period;
        }
    }
}
void calculateRSI(StockData& data, int period = 14) {
    data.rsi_14.resize(data.close.size());

    vector<double> gains, losses;
    for (size_t i = 1; i < data.close.size(); ++i) {
        double change = data.close[i] - data.close[i-1];
        gains.push_back(change > 0 ? change : 0);
        losses.push_back(change < 0 ? -change : 0);
    }

    for (size_t i = 0; i < data.close.size(); ++i) {
        if (i < period) {
            data.rsi_14[i] = 50.0;
        } else {
            double avg_gain = 0.0, avg_loss = 0.0;
            for (int j = 0; j < period; ++j) {
                avg_gain += gains[i - period + j];
                avg_loss += losses[i - period + j];
            }
            avg_gain /= period;
            avg_loss /= period;

            if (avg_loss == 0) {
                data.rsi_14[i] = 100.0;
            } else {
                double rs = avg_gain / avg_loss;
                data.rsi_14[i] = 100.0 - (100.0 / (1.0 + rs));
            }
        }
    }
}

void calculateMomentum(StockData& data) {
    int periods[] = {20, 60, 120};
    vector<vector<double>*> momentum_features = {&data.momentum_1m, &data.momentum_3m, &data.momentum_6m};

    for (int i = 0; i < 3; ++i) {
        int period = periods[i];
        auto& feature = *momentum_features[i];
        feature.resize(data.returns.size());
        for (size_t j = 0; j < data.returns.size(); ++j) {
            if (j < period) {
                feature[j] = 0.0;
            } else {
                double cum_return = 1.0;
                for (int k = 1; k <= period; ++k) {
                    cum_return *= (1.0 + data.returns[j - k + 1]);
                }
                feature[j] = cum_return - 1.0;
            }
        }
    }
}

void calculateVolatility(StockData& data, int period = 20) {
    data.volatility_20.resize(data.returns.size());
    for (size_t i = 0; i < data.returns.size(); ++i) {
        if (i < period) {
            data.volatility_20[i] = 0.0;
        } else {
            vector<double> window(data.returns.begin() + i - period + 1, data.returns.begin() + i + 1);
            double mean = accumulate(window.begin(), window.end(), 0.0)/ period;
            double variance = 0.0;
            for (double r : window) {
                variance += (r - mean) * (r - mean);
            }
            variance /= period;
            data.volatility_20[i] = sqrt(variance);
        }
    }
}

StockData preprocessStock(const string& symbol, const string& filepath) {
    auto start = chrono::high_resolution_clock::now();
    StockData data;
    data.symbol = symbol;
    if (!loadStockData(filepath, data)) {
        return data;
    }

    auto load_end = chrono::high_resolution_clock::now();
    auto load_duration = chrono::duration_cast<chrono::milliseconds>(load_end - start);

    calculateReturns(data);
    calculateSMA(data);
    calculateRSI(data);
    calculateMomentum(data);
    calculateVolatility(data);

    auto process_end = chrono::high_resolution_clock::now();
    auto process_duration = chrono::duration_cast<chrono::milliseconds>(process_end - load_end);
    auto total_duration = chrono::duration_cast<chrono::milliseconds>(process_end - start);

    cout << "Processed " << symbol << ": " << data.dates.size() << " days" << endl;
    cout << "  Load time: " << load_duration.count() << " ms" << endl;
    cout << "  Process time: " << process_duration.count() << " ms" << endl;
    cout << "  Total time: " << total_duration.count() << " ms" << endl;

    return data;
}

void walkForwardValidation(const vector<StockData>& stocks, Dataset& dataset) {

    if (stocks.empty()) return;
    size_t total_days = stocks[0].dates.size();
    size_t min_train_days = 120;
    size_t test_window = 20;
    for (size_t test_start = min_train_days; test_start + test_window <= total_days; test_start += test_window) {
        size_t train_end = test_start;
        size_t test_end = test_start + test_window;
        dataset.X_train.insert(dataset.X_train.end(), dataset.X.begin() + train_end - min_train_days, dataset.X.begin() + train_end);
        dataset.Y_train.insert(dataset.Y_train.end(), dataset.Y.begin() + train_end - min_train_days, dataset.Y.begin() + train_end);
        dataset.X_test.insert(dataset.X_test.end(), dataset.X.begin() + test_start, dataset.X.begin() + test_end);
        dataset.Y_test.insert(dataset.Y_test.end(), dataset.Y.begin() + test_start, dataset.Y.begin() + test_end);
    }
}

int main() {
    const string data_dir = "../data/";
    vector<string> stock_files = {
        "AAPL.csv", "AMZN.csv", "GOOG.csv", "JPM.csv",
        "META.csv", "MSFT.csv", "NVDA.csv", "TSLA.csv", "UNH.csv", "XOM.csv"
    };

    ThreadPool pool(4, stock_files.size());
    vector<StockData> all_stocks;
    mutex stocks_mutex;

    for (const auto& filename : stock_files) {
        std::string symbol = filename.substr(0, filename.find('.'));
        std::string filepath = data_dir + filename;

        pool.enqueue([&]() {
            StockData data = preprocessStock(symbol, filepath);
            lock_guard<mutex> lock(stocks_mutex);
            all_stocks.push_back(move(data));
        });
    }
    pool.request_stop();

    cout << "\nAll stock data preprocessing completed!" << endl;
    cout << "Total stocks processed: " << all_stocks.size() << endl;

    walkForwardValidation(all_stocks, dataset);

    // TODO: Next step - integrate XGBoost for model training

    return 0;
}

