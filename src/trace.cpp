#include "trace.h"

namespace xi {

Trace *Trace::shared() {
    static Trace trace;
    return &trace;
}

bool Trace::isEnabled() {
    UnfairLocker locker(m_mutex);
    return m_enabled;
}

void Trace::setEnabled(bool enabled) {
    UnfairLocker locker(m_mutex);
    m_enabled = enabled;
    m_n_entries = 0;
}

void Trace::trace(const QString &name, TraceCategory cat, TracePhase ph) {
    using namespace std::chrono;
    // const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).count();
    // milliseconds microseconds
    // https://www.guyrutenberg.com/2013/01/27/using-stdchronohigh_resolution_clock-example/
    // https://stackoverflow.com/questions/19555121/how-to-get-current-timestamp-in-milliseconds-since-1970-just-the-way-java-gets
    auto timestamp = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();

    UnfairLocker locker(m_mutex);
    if (!m_enabled) return;
    auto i = m_n_entries % BUF_SIZE;
    m_buf->at(i).name = name;
    m_buf->at(i).cat = cat;
    m_buf->at(i).ph = ph;
    m_buf->at(i).abstime = timestamp;
    // https://stackoverflow.com/questions/19854932/is-there-a-portable-way-to-give-thread-name-with-qt
    // buf[i].thread_name =
    m_buf->at(i).tid = (quint64)QThread::currentThreadId();
    m_n_entries++;
}

QJsonDocument Trace::json() {
    UnfairLocker locker(m_mutex);
    auto pid = QCoreApplication::applicationPid();

    QJsonArray arr;
    QJsonDocument result;
    QJsonObject obj;
    obj["name"] = "process_name";
    obj["ph"] = "M";
    obj["pid"] = pid;
    obj["tid"] = m_buf->at(0).tid;
    obj["ts"] = m_buf->at(0).tid;
    QJsonObject args;
    args["name"] = "xi-mac";
    obj["args"] = args;
    arr.append(obj);

    for (auto entry_num = std::max(0, m_n_entries - BUF_SIZE); entry_num < m_n_entries; ++entry_num) {
        auto i = entry_num % BUF_SIZE;
        auto ts = m_buf->at(i).abstime;
        if (m_buf->at(i).thread_name.size() != 0) {
            QJsonObject obj;
            obj["name"] = "process_name";
            obj["ph"] = "M";
            obj["pid"] = pid;
            obj["tid"] = m_buf->at(i).tid;
            obj["ts"] = ts;
            QJsonObject args;
            args["name"] = "";
            obj["args"] = args;
            arr.append(obj);
        }
        QJsonObject obj;
        obj["name"] = m_buf->at(i).name;
        obj["cat"] = to_string(m_buf->at(i).cat);
        obj["ph"] = to_string(m_buf->at(i).ph);
        obj["pid"] = pid;
        obj["tid"] = m_buf->at(i).tid;
        obj["ts"] = ts;
        arr.append(obj);
    }
    result.setArray(arr);
    return result;
}

QJsonDocument Trace::snapshot() {
    return json();
}

Trace::Trace() {
    m_buf = std::make_unique<std::vector<TraceEntry>>(BUF_SIZE);
}

// https://stackoverflow.com/questions/18837857/cant-use-enum-class-as-unordered-map-key
struct EnumClassHash {
    template <typename T>
    std::size_t operator()(T t) const {
        return static_cast<std::size_t>(t);
    }
};

QString to_string(TraceCategory tc) {
    static std::unordered_map<TraceCategory, QString, EnumClassHash> xmap;
    if (xmap.size() == 0) {
        xmap[TraceCategory::Main] = "main";
        xmap[TraceCategory::Rpc] = "rpc";
    }
    return xmap[tc];
}

QString to_string(TracePhase tp) {
    static std::unordered_map<TracePhase, QString, EnumClassHash> xmap;
    if (xmap.size() == 0) {
        xmap[TracePhase::Begin] = "B";
        xmap[TracePhase::End] = "E";
        xmap[TracePhase::Instant] = "I";
    }
    return xmap[tp];
}

} // namespace xi
