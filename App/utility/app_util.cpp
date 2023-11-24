# include "app_util.h"

#include "../App.h"
#include "Enclave_u.h"

/**
 * @brief タイムスタンプの追加
 * 
 * 現在のシステム時刻をタイムスタンプとしてベクターに追加する
 */
void DisplayResults::addTimestamp() {
    timestamps.emplace_back(std::chrono::system_clock::now());
}

/**
 * @brief 各種のシステムパラメータを表示する。
 * 
 * @param CLOCKS_PER_US ターゲットハードウェアにおけるマイクロ秒ごとのクロック数
 * @param EPOCH_TIME    エポックの期間（ミリ秒単位）
 * @param EXTIME        実行時間（秒単位）
 * @param MAX_OPE       操作の最大数
 * @param RRAITO        読み取り操作の比率（パーセンテージ）
 * @param WORKER_NUM    使用されるスレッドの総数
 * @param TUPLE_NUM     タプルの総数
 * @param YCSB          YCSB (Yahoo! Cloud Serving Benchmark) を使用するかどうかを示すフラグ
 * @param ZIPF_SKEW     Zipf分布の歪度（0は一様分布を示す）
 * @param LOGGER_NUM    ロガースレッドの数
 * @param BUFFER_NUM    バッファの数
 */
void DisplayResults::displayParameter() {
    std::cout << "#clocks_per_us:\t"<< CLOCKS_PER_US<< std::endl;
    std::cout << "#epoch_time:\t"   << EPOCH_TIME   << std::endl;
    std::cout << "#extime:\t"       << EXTIME       << std::endl;
    std::cout << "#max_ope:\t"      << MAX_OPE      << std::endl;
    std::cout << "#rratio:\t"       << RRAITO       << std::endl;
    std::cout << "#thread_num:\t"   << WORKER_NUM   << std::endl;
    std::cout << "#tuple_num:\t"    << TUPLE_NUM    << std::endl;
    std::cout << "#ycsb:\t\t"       << YCSB         << std::endl;
    std::cout << "#zipf_skew:\t"    << ZIPF_SKEW    << std::endl;
    std::cout << "#logger_num:\t"   << LOGGER_NUM   << std::endl;
    std::cout << "#buffer_num:\t"   << BUFFER_NUM   << std::endl;
}

/**
 * @brief 複数のスレッドによる実行結果を集計し、結果を表示する
 * 
 * 各スレッドによるコミット数、アボート数、各アボートの理由に応じたアボート数を集計する
 * SHOW_DETAILS が有効な場合、詳細な結果も表示される
 * 
 * @param SiloResult 各スレッドの実行結果を保持する配列
 * @param WORKER_NUM スレッドの総数
 * @param total_commit_counts_ 各スレッドのコミット数の合計
 * @param total_abort_counts_ 各スレッドのアボート数の合計
 * @param total_abort_res_counts_ 各アボートの理由に応じたアボート数の合計
 * @param EXTIME 実行時間（秒単位）
 */
void DisplayResults::displayResult() {
    // ====================
    // logger result
    // ====================
    double cps = CLOCKS_PER_US*1e6;
    for (size_t i = 0; i < LOGGER_NUM; i++) {
    #if SHOW_DETAILS
        std::cout << ("Logger#" + std::to_string(i))
            << " byte_count[B]="    << loggerResults[i].byte_count_
            << " write_latency[s]=" << loggerResults[i].write_latency_/cps
            << " throughput[B/s]="  << loggerResults[i].byte_count_/(loggerResults[i].write_latency_/cps)
            << " wait_latency[s]="  << loggerResults[i].wait_latency_/cps
        << std::endl;
    #endif
    }
    // ====================
    // worker result
    // ====================
    // TODO: tablePrinterみたいなやつを作る
    std::cout << std::left
        << std::setw(8) << "Thread#"
        << "| " << std::setw(12) << "Commit"
        << "| " << std::setw(12) << "Abort"
        << "| " << std::setw(8) << "VP1"
        << "| " << std::setw(8) << "VP2"
        << "| " << std::setw(8) << "VP3"
        << "| " << std::setw(8) << "NullB"
    << std::endl;
    std::cout << std::string(76, '-') << std::endl; // 区切り線

    for (size_t i = 0; i < workerResults.size(); i++) {
    #if SHOW_DETAILS
        // 各ワーカースレッドのcommit/abort数と、abort理由(vp1/vp2/vp3/nullBuffer)を表示する
        std::cout << std::left 
            << std::setw(8) << ("#" + std::to_string(i)) 
            << "| " << std::setw(12) << workerResults[i].local_commit_count_ 
            << "| " << std::setw(12) << workerResults[i].local_abort_count_ 
            << "| " << std::setw(8)  << workerResults[i].local_abort_vp1_count_ 
            << "| " << std::setw(8)  << workerResults[i].local_abort_vp2_count_ 
            << "| " << std::setw(8)  << workerResults[i].local_abort_vp3_count_ 
            << "| " << std::setw(8)  << workerResults[i].local_abort_nullBuffer_count_ 
        << std::endl;
    #endif
        // 各ワーカースレッドの数値の総和を計算する
        total_commit_count_           += workerResults[i].local_commit_count_;
        total_abort_count_            += workerResults[i].local_abort_count_;
        total_abort_vp1_count_        += workerResults[i].local_abort_vp1_count_;
        total_abort_vp2_count_        += workerResults[i].local_abort_vp2_count_;
        total_abort_vp3_count_        += workerResults[i].local_abort_vp3_count_;
        total_abort_nullBuffer_count_ += workerResults[i].local_abort_nullBuffer_count_;
    }
    #if SHOW_DETAILS
    // 算出した総和の数を元にresultを表示する
    double abort_rate = (double)total_abort_count_ / (double)(total_commit_count_ + total_abort_count_);
    uint64_t throughput = total_commit_count_ / EXTIME;
    long double latency = powl(10.0, 9.0) / throughput * WORKER_NUM;
    std::cout << "===== Transaction Protocol Performance ====="                     << std::endl;
    std::cout << "[info]\tcommit_counts_:\t"       << total_commit_count_           << std::endl;
    std::cout << "[info]\tabort_counts_:\t"        << total_abort_count_            << std::endl;
    std::cout << "[info]\t├─ abort_validation1:\t" << total_abort_vp1_count_        << std::endl;
    std::cout << "[info]\t├─ abort_validation2:\t" << total_abort_vp2_count_        << std::endl;
    std::cout << "[info]\t├─ abort_validation3:\t" << total_abort_vp3_count_        << std::endl;
    std::cout << "[info]\t└─ abort_NULLbuffer:\t"  << total_abort_nullBuffer_count_ << std::endl;
    std::cout << "[info]\tabort_rate:\t"           << abort_rate                    << std::endl;
    std::cout << "[info]\tlatency[ns]:\t"          << latency                       << std::endl;
    std::cout << "[info]\tthroughput[tps]:\t"      << throughput                    << std::endl;

    #ifdef ADD_ANALYSIS
    uint64_t total_read_time = 0;
    uint64_t total_read_internal_time = 0;
    uint64_t total_write_time = 0;
    uint64_t total_validation_time = 0;
    uint64_t total_write_phase_time = 0;
    uint64_t total_masstree_read_get_value_time = 0;
    uint64_t total_durable_epoch_work_time = 0;
    uint64_t total_make_procedure_time = 0;
    uint64_t total_masstree_write_get_value_time = 0;

    for (size_t i = 0; i < workerResults.size(); i++) {
        total_read_time += workerResults[i].local_read_time_;
        total_read_internal_time += workerResults[i].local_read_internal_time_;
        total_write_time += workerResults[i].local_write_time_;
        total_validation_time += workerResults[i].local_validation_time_;
        total_write_phase_time += workerResults[i].local_write_phase_time_;
        total_masstree_read_get_value_time += workerResults[i].local_masstree_read_get_value_time_;
        total_durable_epoch_work_time += workerResults[i].local_durable_epoch_work_time_;
        total_make_procedure_time += workerResults[i].local_make_procedure_time_;
        total_masstree_write_get_value_time += workerResults[i].local_masstree_write_get_value_time_;
    }

    uint64_t total_time = total_read_time + 
                          total_write_time + 
                          total_validation_time + 
                          total_write_phase_time + 
                          total_durable_epoch_work_time + 
                          total_make_procedure_time;

    std::cout << "===== Transaction Protocol Performance ====="                             << std::endl;
    std::cout << "[info]\tread_time(-inter-read_get):\t"  << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_read_time - total_read_internal_time - total_masstree_read_get_value_time) / total_time) * 100.0 << "%)\t" << total_read_time - total_read_internal_time - total_masstree_read_get_value_time << "(" << total_read_time << ")" << std::endl;
    std::cout << "[info]\tread_internal_time:\t\t"     << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_read_internal_time) / total_time) * 100.0 << "%)\t" << total_read_internal_time         << std::endl;
#if INDEX_PATTERN == INDEX_USE_MASSTREE
    std::cout << "[info]\tmasstree_read_get_value_time:\t"  << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_masstree_read_get_value_time) / total_time) * 100.0 << "%)\t" << total_masstree_read_get_value_time    << std::endl;
#elif INDEX_PATTERN == INDEX_USE_OCH
    std::cout << "[info]\toptcuckoo_read_get_value_time:\t" << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_masstree_read_get_value_time) / total_time) * 100.0 << "%)\t" << total_masstree_read_get_value_time    << std::endl;
#endif
    std::cout << "[info]\twrite_time(-write_get):\t\t"           << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_write_time - total_masstree_write_get_value_time) / total_time) * 100.0 << "%)\t" << total_write_time - total_masstree_write_get_value_time << "(" << total_write_time << ")" << std::endl;
#if INDEX_PATTERN == INDEX_USE_MASSTREE
    std::cout << "[info]\tmasstree_write_get_value_time:\t"  << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_masstree_write_get_value_time) / total_time) * 100.0 << "%)\t" << total_masstree_write_get_value_time    << std::endl;
#elif INDEX_PATTERN == INDEX_USE_OCH
    std::cout << "[info]\toptcuckoo_write_get_value_time:\t" << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_masstree_write_get_value_time) / total_time) * 100.0 << "%)\t" << total_masstree_write_get_value_time    << std::endl;
#endif
    std::cout << "[info]\tvalidation_time:\t\t"        << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_validation_time) / total_time) * 100.0 << "%)\t" << total_validation_time            << std::endl;
    std::cout << "[info]\twrite_phase_time:\t\t"       << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_write_phase_time) / total_time) * 100.0 << "%)\t" << total_write_phase_time           << std::endl;
    std::cout << "[info]\tdurable_epoch_work_time:\t"  << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_durable_epoch_work_time) / total_time) * 100.0 << "%)\t" << total_durable_epoch_work_time    << std::endl;
    std::cout << "[info]\tmake_procedure_time:\t\t"    << "(" << std::fixed << std::setprecision(3) << (static_cast<double>(total_make_procedure_time) / total_time) * 100.0 << "%)\t" << total_make_procedure_time        << std::endl;


    double total_time_s = static_cast<double>(total_time) / (CLOCKS_PER_US * 1000000.0);
    std::cout << "[info]\ttotal_time(approx.):\t\t" << total_time_s / WORKER_NUM << " s" << std::endl;

    #endif

    assert(timestamps.size() == 5);
    double makedb_time    = calculateDurationTime_ms(0, 1);
    double createth_time  = calculateDurationTime_ms(1, 2);
    double ex_time        = calculateDurationTime_ms(2, 3);
    double destroyth_time = calculateDurationTime_ms(3, 4);

    std::cout << "===== Enclave Performance ====="                             << std::endl;
    std::cout << "[info]\tmakeDB:\t"            << makedb_time/1000    << "s." << std::endl;
    std::cout << "[info]\tcreateThread:\t"      << createth_time/1000  << "s." << std::endl;
    std::cout << "[info]\texecutionTime:\t"     << ex_time/1000        << "s." << std::endl;
    std::cout << "[info]\tdestroyThread:\t"     << destroyth_time/1000 << "s." << std::endl;
    // std::cout << "[info]\tcall_count(write):\t" << ocall_count.load()          << std::endl;
    // std::cout << "[info]\tdurableEpoch:\t"      << ret_durableEpoch            << std::endl;
    #endif
    std::cout << "=== for copy&paste ===" << std::endl;
    std::cout << total_commit_count_      << std::endl; // commit count
    std::cout << total_abort_count_       << std::endl; // abort count
    std::cout << abort_rate               << std::endl; // abort rate
    std::cout << latency                  << std::endl; // latency
    // std::cout << ret_durableEpoch         << std::endl; // durable epoch
    std::cout << throughput               << std::endl; // throughput
}

/**
 * @brief 指定された開始時間と終了時間のインデックスに基づいて、経過時間をミリ秒単位で計算する
 * 
 * @param startTimeIndex 開始時間のインデックス
 * @param endTimeIndex 終了時間のインデックス
 * @return 経過時間（ミリ秒単位）
 * 
 * @note 関数は、内部で保持されているtimestampsベクトルを使用して時間の差を計算する
 *       また、指定されたインデックスが適切な範囲にあることを確認するためのアサーションも含まれている
 */
double DisplayResults::calculateDurationTime_ms(size_t startTimeIndex, size_t endTimeIndex) {
    assert(startTimeIndex < endTimeIndex);
    assert(endTimeIndex <= timestamps.size());
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(timestamps[endTimeIndex] - timestamps[startTimeIndex]).count() / 1000.0);
}

void DisplayResults::getWorkerResult() {
    uint64_t ret = 0;
    for (int i = 0; i < WORKER_NUM; i++) {
        ecall_getCommitCount(global_eid, &ret, i);
        workerResults[i].local_commit_count_ = ret;
        ecall_getAbortCount(global_eid, &ret, i);
        workerResults[i].local_abort_count_ = ret;
        ecall_getSpecificAbortCount(global_eid, &ret, i, ValidationPhase1);
        workerResults[i].local_abort_vp1_count_ = ret;
        ecall_getSpecificAbortCount(global_eid, &ret, i, ValidationPhase2);
        workerResults[i].local_abort_vp2_count_ = ret;
        ecall_getSpecificAbortCount(global_eid, &ret, i, ValidationPhase3);
        workerResults[i].local_abort_vp3_count_ = ret;
        ecall_getSpecificAbortCount(global_eid, &ret, i, NullCurrentBuffer);
        workerResults[i].local_abort_nullBuffer_count_ = ret;
#ifdef ADD_ANALYSIS
        ecall_get_analysis(global_eid, &ret, i, 0);
        workerResults[i].local_read_time_ = ret;
        ecall_get_analysis(global_eid, &ret, i, 1);
        workerResults[i].local_read_internal_time_ = ret;
        ecall_get_analysis(global_eid, &ret, i, 2);
        workerResults[i].local_write_time_ = ret;
        ecall_get_analysis(global_eid, &ret, i, 3);
        workerResults[i].local_validation_time_ = ret;
        ecall_get_analysis(global_eid, &ret, i, 4);
        workerResults[i].local_write_phase_time_ = ret;
        ecall_get_analysis(global_eid, &ret, i, 5);
        workerResults[i].local_masstree_read_get_value_time_= ret;
        ecall_get_analysis(global_eid, &ret, i, 6);
        workerResults[i].local_durable_epoch_work_time_= ret;
        ecall_get_analysis(global_eid, &ret, i, 7);
        workerResults[i].local_make_procedure_time_ = ret;
        ecall_get_analysis(global_eid, &ret, i, 8);
        workerResults[i].local_masstree_write_get_value_time_ = ret;
#endif
    }
}

void DisplayResults::getLoggerResult() {
    uint64_t ret = 0;
    for (int i = 0; i < LOGGER_NUM; i++) {
        ecall_getLoggerCount(global_eid, &ret, i, LoggerResultType::ByteCount);
        loggerResults[i].byte_count_ = ret;
        ecall_getLoggerCount(global_eid, &ret, i, LoggerResultType::WriteLatency);
        loggerResults[i].write_latency_ = ret;
        ecall_getLoggerCount(global_eid, &ret, i, LoggerResultType::WaitLatency);
        loggerResults[i].wait_latency_ = ret;
    }
}