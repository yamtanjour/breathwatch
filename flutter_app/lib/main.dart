import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import 'package:intl/intl.dart';
import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(const MyApp());
}

// ─── Data model ───────────────────────────────────────────────────────────────

class AirSample {
  final DateTime timestamp;
  final double temp;
  final double humidity;
  final int aqi;
  final int tvoc;
  final int eco2;

  AirSample({
    required this.timestamp,
    required this.temp,
    required this.humidity,
    required this.aqi,
    required this.tvoc,
    required this.eco2,
  });

  Map<String, dynamic> toJson() => {
        'ts': timestamp.toIso8601String(),
        'temp': temp,
        'humidity': humidity,
        'aqi': aqi,
        'tvoc': tvoc,
        'eco2': eco2,
      };

  factory AirSample.fromJson(Map<String, dynamic> j) => AirSample(
        timestamp: DateTime.parse(j['ts']),
        temp: (j['temp'] as num).toDouble(),
        humidity: (j['humidity'] as num).toDouble(),
        aqi: (j['aqi'] as num).toInt(),
        tvoc: (j['tvoc'] as num).toInt(),
        eco2: (j['eco2'] as num).toInt(),
      );
}

// ─── Session model (one download = one session) ──────────────────────────────

class Session {
  final DateTime downloadedAt;
  final List<AirSample> samples;

  Session({required this.downloadedAt, required this.samples});

  Map<String, dynamic> toJson() => {
        'downloadedAt': downloadedAt.toIso8601String(),
        'samples': samples.map((s) => s.toJson()).toList(),
      };

  factory Session.fromJson(Map<String, dynamic> j) => Session(
        downloadedAt: DateTime.parse(j['downloadedAt']),
        samples: (j['samples'] as List)
            .map((s) => AirSample.fromJson(s as Map<String, dynamic>))
            .toList(),
      );
}

// ─── Local persistence ───────────────────────────────────────────────────────

Future<File> _storageFile() async {
  final dir = await getApplicationDocumentsDirectory();
  return File('${dir.path}/breathwatch_sessions.json');
}

Future<List<Session>> loadSessions() async {
  try {
    final f = await _storageFile();
    if (!await f.exists()) return [];
    final raw = await f.readAsString();
    final list = jsonDecode(raw) as List;
    return list
        .map((e) => Session.fromJson(e as Map<String, dynamic>))
        .toList();
  } catch (_) {
    return [];
  }
}

Future<void> saveSessions(List<Session> sessions) async {
  final f = await _storageFile();
  await f.writeAsString(jsonEncode(sessions.map((s) => s.toJson()).toList()));
}

// ─── App ─────────────────────────────────────────────────────────────────────

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BreathWatch',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorSchemeSeed: Colors.blueAccent,
        useMaterial3: true,
        scaffoldBackgroundColor: const Color(0xFFF5F7FA),
      ),
      home: const HomePage(),
    );
  }
}

// ─── Home page ───────────────────────────────────────────────────────────────

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  List<Session> sessions = [];
  bool downloading = false;
  String status = '';
  StringBuffer dataBuffer = StringBuffer();
  BluetoothConnection? connection;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    sessions = await loadSessions();
    setState(() {});
  }

  // ── Bluetooth download ──────────────────────────────────────────────────

  Future<void> _startDownload() async {
    final statuses = await [
      Permission.bluetoothConnect,
      Permission.bluetoothScan,
      Permission.location,
    ].request();

    if (statuses[Permission.bluetoothConnect] != PermissionStatus.granted) {
      _setStatus('Bluetooth permission denied');
      return;
    }

    final bonded =
        await FlutterBluetoothSerial.instance.getBondedDevices();
    if (bonded.isEmpty) {
      _setStatus('No paired devices found. Pair the HC-05 first.');
      return;
    }

    if (!mounted) return;

    final device = await showDialog<BluetoothDevice>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Select Device'),
        content: SizedBox(
          width: double.maxFinite,
          child: ListView(
            shrinkWrap: true,
            children: bonded.map((d) {
              return ListTile(
                title: Text(d.name ?? 'Unknown'),
                subtitle: Text(d.address),
                onTap: () => Navigator.of(ctx).pop(d),
              );
            }).toList(),
          ),
        ),
      ),
    );

    if (device == null) return;
    setState(() {
      downloading = true;
      status = 'Connecting to ${device.name}…';
    });
    dataBuffer.clear();

    try {
      connection =
          await BluetoothConnection.toAddress(device.address);
      _setStatus('Connected – waiting for data…');

      connection!.input!.listen(
        (Uint8List data) {
          final chunk = String.fromCharCodes(data);
          dataBuffer.write(chunk);
          final buf = dataBuffer.toString();

          if (buf.contains('END\r\n')) {
            final endIdx = buf.indexOf('END\r\n');
            final payload = buf.substring(0, endIdx);
            _processPayload(payload);
            dataBuffer.clear();
            connection?.close();
          }
        },
        onDone: () {
          if (downloading) {
            final buf = dataBuffer.toString().trim();
            if (buf.isNotEmpty) _processPayload(buf);
            setState(() => downloading = false);
          }
        },
        onError: (_) {
          _setStatus('Connection error');
          setState(() => downloading = false);
        },
      );
    } catch (e) {
      _setStatus('Connection failed');
      setState(() => downloading = false);
    }
  }

  void _processPayload(String payload) {
    final lines = payload.split('\r\n');
    final validLines = <String>[];
    for (final line in lines) {
      final trimmed = line.trim();
      if (trimmed.isEmpty) continue;
      final parts = trimmed.split(',');
      if (parts.length == 5) validLines.add(trimmed);
    }

    if (validLines.isEmpty) {
      _setStatus('No valid data received');
      setState(() => downloading = false);
      return;
    }

    final now = DateTime.now();
    final sampleList = <AirSample>[];
    for (var i = 0; i < validLines.length; i++) {
      final parts = validLines[i].split(',');
      try {
        sampleList.add(AirSample(
          timestamp:
              now.subtract(Duration(seconds: 12 * (validLines.length - 1 - i))),
          temp: double.parse(parts[0]),
          humidity: double.parse(parts[1]),
          aqi: int.parse(parts[2]),
          tvoc: int.parse(parts[3]),
          eco2: int.parse(parts[4]),
        ));
      } catch (_) {}
    }

    final session = Session(downloadedAt: now, samples: sampleList);
    sessions.add(session);
    saveSessions(sessions);
    _setStatus('Download complete – ${sampleList.length} samples');
    setState(() => downloading = false);
  }

  void _setStatus(String s) {
    if (mounted) setState(() => status = s);
  }

  // ── Delete session ──────────────────────────────────────────────────────

  Future<void> _deleteSession(int index) async {
    sessions.removeAt(index);
    await saveSessions(sessions);
    setState(() {});
  }

  // ── Build ───────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    // Flatten all samples for "latest" card
    final allSamples =
        sessions.expand((s) => s.samples).toList()
          ..sort((a, b) => a.timestamp.compareTo(b.timestamp));
    final latest = allSamples.isNotEmpty ? allSamples.last : null;

    return Scaffold(
      appBar: AppBar(
        title: const Text('BreathWatch'),
        centerTitle: true,
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // ── Download button ────────────────────────────────────────────
          SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              onPressed: downloading ? null : _startDownload,
              icon: downloading
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(strokeWidth: 2))
                  : const Icon(Icons.bluetooth),
              label: Text(downloading ? 'Downloading…' : 'Download from Device'),
              style: ElevatedButton.styleFrom(
                padding: const EdgeInsets.symmetric(vertical: 16),
              ),
            ),
          ),
          if (status.isNotEmpty)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(status,
                  textAlign: TextAlign.center,
                  style: TextStyle(color: Colors.grey[600])),
            ),

          const SizedBox(height: 24),

          // ── Latest reading card ────────────────────────────────────────
          if (latest != null) ...[
            const Text('Latest Reading',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            _LatestCard(sample: latest),
            const SizedBox(height: 24),
          ],

          // ── Past sessions ──────────────────────────────────────────────
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              const Text('Past Sessions',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
              Text('${sessions.length} session(s)',
                  style: TextStyle(color: Colors.grey[600])),
            ],
          ),
          const SizedBox(height: 8),
          if (sessions.isEmpty)
            const Card(
              child: Padding(
                padding: EdgeInsets.all(24),
                child: Text('No data yet. Download from your device!',
                    textAlign: TextAlign.center),
              ),
            )
          else
            ...sessions.reversed.toList().asMap().entries.map((entry) {
              final reversedIdx = entry.key;
              final session = entry.value;
              final realIdx = sessions.length - 1 - reversedIdx;
              final fmt = DateFormat('MMM d, yyyy – HH:mm');
              final dur = session.samples.length * 12;
              final durStr = dur > 3600
                  ? '${(dur / 3600).toStringAsFixed(1)} h'
                  : dur > 60
                      ? '${(dur / 60).toStringAsFixed(0)} min'
                      : '$dur s';
              return Card(
                child: ListTile(
                  title: Text(fmt.format(session.downloadedAt)),
                  subtitle: Text(
                      '${session.samples.length} samples · ~$durStr span'),
                  trailing: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      IconButton(
                        icon: const Icon(Icons.delete_outline, size: 20),
                        onPressed: () async {
                          final confirm = await showDialog<bool>(
                            context: context,
                            builder: (ctx) => AlertDialog(
                              title: const Text('Delete Session?'),
                              content: const Text(
                                  'This will permanently remove this session.'),
                              actions: [
                                TextButton(
                                    onPressed: () =>
                                        Navigator.pop(ctx, false),
                                    child: const Text('Cancel')),
                                TextButton(
                                    onPressed: () =>
                                        Navigator.pop(ctx, true),
                                    child: const Text('Delete')),
                              ],
                            ),
                          );
                          if (confirm == true) _deleteSession(realIdx);
                        },
                      ),
                      const Icon(Icons.chevron_right),
                    ],
                  ),
                  onTap: () => Navigator.of(context).push(MaterialPageRoute(
                    builder: (_) => SessionDetailPage(session: session),
                  )),
                ),
              );
            }),
        ],
      ),
    );
  }
}

// ─── Latest‑reading card ─────────────────────────────────────────────────────

class _LatestCard extends StatelessWidget {
  final AirSample sample;
  const _LatestCard({required this.sample});

  @override
  Widget build(BuildContext context) {
    final fmt = DateFormat('MMM d, HH:mm:ss');
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            Text(fmt.format(sample.timestamp),
                style: TextStyle(color: Colors.grey[600], fontSize: 13)),
            const SizedBox(height: 12),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceAround,
              children: [
                _metric('Temp', '${sample.temp.toStringAsFixed(1)}°C',
                    Colors.orange),
                _metric('Humidity', '${sample.humidity.toStringAsFixed(1)}%',
                    Colors.blue),
                _metric('AQI', '${sample.aqi}', _aqiColor(sample.aqi)),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceAround,
              children: [
                _metric('TVOC', '${sample.tvoc} ppb', Colors.purple),
                _metric('eCO₂', '${sample.eco2} ppm', Colors.teal),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _metric(String label, String value, Color color) {
    return Column(
      children: [
        Text(label, style: TextStyle(fontSize: 12, color: Colors.grey[600])),
        const SizedBox(height: 4),
        Text(value,
            style: TextStyle(
                fontSize: 20, fontWeight: FontWeight.bold, color: color)),
      ],
    );
  }

  Color _aqiColor(int aqi) {
    if (aqi <= 1) return Colors.green;
    if (aqi <= 2) return Colors.yellow[800]!;
    if (aqi <= 3) return Colors.orange;
    if (aqi <= 4) return Colors.red;
    return Colors.purple;
  }
}

// ─── Session detail page (graphs + data table) ───────────────────────────────

class SessionDetailPage extends StatelessWidget {
  final Session session;
  const SessionDetailPage({super.key, required this.session});

  @override
  Widget build(BuildContext context) {
    final samples = session.samples;
    final fmt = DateFormat('MMM d, yyyy – HH:mm');
    return DefaultTabController(
      length: 2,
      child: Scaffold(
        appBar: AppBar(
          title: Text(fmt.format(session.downloadedAt)),
          bottom: const TabBar(tabs: [
            Tab(text: 'Graphs'),
            Tab(text: 'All Data'),
          ]),
        ),
        body: TabBarView(children: [
          _GraphsTab(samples: samples),
          _DataTab(samples: samples),
        ]),
      ),
    );
  }
}

// ─── Graphs tab ──────────────────────────────────────────────────────────────

class _GraphsTab extends StatelessWidget {
  final List<AirSample> samples;
  const _GraphsTab({required this.samples});

  @override
  Widget build(BuildContext context) {
    if (samples.isEmpty) {
      return const Center(child: Text('No data'));
    }
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _buildChart('Temperature (°C)', Colors.orange,
            samples.map((s) => s.temp).toList()),
        const SizedBox(height: 24),
        _buildChart('Humidity (%)', Colors.blue,
            samples.map((s) => s.humidity).toList()),
        const SizedBox(height: 24),
        _buildChart('AQI', Colors.green,
            samples.map((s) => s.aqi.toDouble()).toList()),
        const SizedBox(height: 24),
        _buildChart('TVOC (ppb)', Colors.purple,
            samples.map((s) => s.tvoc.toDouble()).toList()),
        const SizedBox(height: 24),
        _buildChart('eCO₂ (ppm)', Colors.teal,
            samples.map((s) => s.eco2.toDouble()).toList()),
      ],
    );
  }

  Widget _buildChart(String title, Color color, List<double> values) {
    final spots = <FlSpot>[];
    for (var i = 0; i < values.length; i++) {
      spots.add(FlSpot(i.toDouble(), values[i]));
    }
    final minY = values.reduce((a, b) => a < b ? a : b);
    final maxY = values.reduce((a, b) => a > b ? a : b);
    final padding = (maxY - minY) * 0.1 + 0.5;

    final totalMinutes = (values.length * 12) / 60;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title,
            style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
        Text(
            '${totalMinutes.toStringAsFixed(0)} min span · ${values.length} samples',
            style: TextStyle(fontSize: 12, color: Colors.grey[600])),
        const SizedBox(height: 8),
        SizedBox(
          height: 200,
          child: LineChart(
            LineChartData(
              gridData: FlGridData(
                show: true,
                drawVerticalLine: false,
                horizontalInterval:
                    _niceInterval(minY - padding, maxY + padding),
              ),
              titlesData: FlTitlesData(
                topTitles: const AxisTitles(
                    sideTitles: SideTitles(showTitles: false)),
                rightTitles: const AxisTitles(
                    sideTitles: SideTitles(showTitles: false)),
                bottomTitles: AxisTitles(
                  sideTitles: SideTitles(
                    showTitles: true,
                    reservedSize: 28,
                    interval:
                        (values.length / 5).ceilToDouble().clamp(1, 9999),
                    getTitlesWidget: (value, _) {
                      final mins = (value * 12 / 60).toStringAsFixed(0);
                      return Text('${mins}m',
                          style: const TextStyle(fontSize: 10));
                    },
                  ),
                ),
                leftTitles: AxisTitles(
                  sideTitles: SideTitles(
                    showTitles: true,
                    reservedSize: 48,
                    interval:
                        _niceInterval(minY - padding, maxY + padding),
                    getTitlesWidget: (value, _) => Text(
                      value.toStringAsFixed(
                          value == value.roundToDouble() ? 0 : 1),
                      style: const TextStyle(fontSize: 10),
                    ),
                  ),
                ),
              ),
              borderData: FlBorderData(show: false),
              minY: minY - padding,
              maxY: maxY + padding,
              lineBarsData: [
                LineChartBarData(
                  spots: spots,
                  isCurved: true,
                  curveSmoothness: 0.2,
                  color: color,
                  barWidth: 2,
                  dotData: FlDotData(
                    show: values.length < 50,
                    getDotPainter: (_, __, ___, ____) =>
                        FlDotCirclePainter(radius: 2, color: color),
                  ),
                  belowBarData: BarAreaData(
                    show: true,
                    color: color.withValues(alpha: 0.1),
                  ),
                ),
              ],
              lineTouchData: LineTouchData(
                touchTooltipData: LineTouchTooltipData(
                  getTooltipItems: (touchedSpots) {
                    return touchedSpots.map((spot) {
                      final mins = (spot.x * 12 / 60).toStringAsFixed(1);
                      return LineTooltipItem(
                        '${spot.y.toStringAsFixed(1)}\n$mins min',
                        TextStyle(
                            color: color, fontWeight: FontWeight.bold),
                      );
                    }).toList();
                  },
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }

  double _niceInterval(double min, double max) {
    final range = max - min;
    if (range <= 0) return 1;
    final rough = range / 5;
    if (rough < 1) return 0.5;
    if (rough < 5) return rough.roundToDouble().clamp(1, 9999).toDouble();
    return ((rough / 5).roundToDouble() * 5);
  }
}

// ─── Data table tab ──────────────────────────────────────────────────────────

class _DataTab extends StatelessWidget {
  final List<AirSample> samples;
  const _DataTab({required this.samples});

  @override
  Widget build(BuildContext context) {
    if (samples.isEmpty) {
      return const Center(child: Text('No data'));
    }
    final fmt = DateFormat('HH:mm:ss');
    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      child: SingleChildScrollView(
        child: DataTable(
          columnSpacing: 16,
          headingRowColor:
              WidgetStateProperty.all(Colors.blueAccent.withValues(alpha: 0.1)),
          columns: const [
            DataColumn(label: Text('#')),
            DataColumn(label: Text('Time')),
            DataColumn(label: Text('Temp °C'), numeric: true),
            DataColumn(label: Text('Hum %'), numeric: true),
            DataColumn(label: Text('AQI'), numeric: true),
            DataColumn(label: Text('TVOC'), numeric: true),
            DataColumn(label: Text('eCO₂'), numeric: true),
          ],
          rows: samples.asMap().entries.map((e) {
            final i = e.key;
            final s = e.value;
            return DataRow(cells: [
              DataCell(Text('${i + 1}')),
              DataCell(Text(fmt.format(s.timestamp))),
              DataCell(Text(s.temp.toStringAsFixed(1))),
              DataCell(Text(s.humidity.toStringAsFixed(1))),
              DataCell(Text('${s.aqi}')),
              DataCell(Text('${s.tvoc}')),
              DataCell(Text('${s.eco2}')),
            ]);
          }).toList(),
        ),
      ),
    );
  }
}
