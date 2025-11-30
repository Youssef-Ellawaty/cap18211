#include <WiFi.h>
#include <WebServer.h>
#include <HX711.h>
#include <ArduinoJson.h>

// Function prototypes
void handleRoot();
void handleGetWeight();
void handleSetTarget();
void handleTare();
void handleCalibrate();
void playBuzzer();


// إعدادات WiFi
const char* ssid = "XUP_Ellawaty";
const char* password = "11112222";

// إعدادات HX711
const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN = 5;
const int BUZZER_PIN = 27;

HX711 scale;
WebServer server(80);

// متغيرات النظام
float calibration_factor = -7050; // قم بتعديل هذا الرقم حسب معايرة الميزان
float targetWeight = 0;
bool targetWeightEnabled = false;
float currentWeight = 0;

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // تهيئة Load Cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); // إعادة تصفير الميزان
  
  // الاتصال بالـ WiFi
  WiFi.begin(ssid, password);
  Serial.print("جاري الاتصال بالـ WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("متصل! العنوان: ");
  Serial.println(WiFi.localIP());
  
  // إعداد مسارات السيرفر
  server.on("/", HTTP_GET, handleRoot);
  server.on("/getWeight", HTTP_GET, handleGetWeight);
  server.on("/setTarget", HTTP_POST, handleSetTarget);
  server.on("/tare", HTTP_POST, handleTare);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  
  server.begin();
  Serial.println("السيرفر يعمل!");
}

void loop() {
  server.handleClient();
  
  // قراءة الوزن
  if (scale.is_ready()) {
    currentWeight = scale.get_units(10);
    
    // فحص الوزن المستهدف
    if (targetWeightEnabled && abs(currentWeight - targetWeight) < 1.0) {
      playBuzzer();
      targetWeightEnabled = false; // تعطيل بعد الوصول
    }
  }
  
  delay(100);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html dir="rtl" lang="ar">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>نظام قياس الوزن</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f0f2f5; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; border-radius: 10px; margin-bottom: 20px; text-align: center; }
        .card { background: white; padding: 20px; margin-bottom: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .weight-display { font-size: 48px; font-weight: bold; color: #667eea; text-align: center; padding: 20px; }
        .btn { background: #667eea; color: white; border: none; padding: 12px 24px; border-radius: 5px; cursor: pointer; font-size: 16px; margin: 5px; }
        .btn:hover { background: #5568d3; }
        .btn-danger { background: #e74c3c; }
        .btn-danger:hover { background: #c0392b; }
        input, select { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; font-size: 16px; }
        .project-item { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 5px; border-right: 4px solid #667eea; }
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th, td { padding: 12px; text-align: right; border-bottom: 1px solid #ddd; }
        th { background: #667eea; color: white; }
        canvas { max-width: 100%; }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-top: 15px; }
        .stat-box { background: #f8f9fa; padding: 15px; border-radius: 5px; text-align: center; }
        .stat-value { font-size: 32px; font-weight: bold; color: #667eea; }
        .stat-label { color: #6c757d; margin-top: 5px; }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔬 نظام قياس الوزن الذكي</h1>
            <p>نظام متكامل لقياس وتحليل الأوزان</p>
        </div>
        
        <div class="card">
            <h2>⚖️ القراءة الحالية</h2>
            <div class="weight-display" id="currentWeight">0.00 g</div>
            <div style="text-align: center;">
                <button class="btn" onclick="tareScale()">إعادة التصفير</button>
                <button class="btn" onclick="saveReading()">حفظ القراءة</button>
            </div>
        </div>
        
        <div class="card">
            <h2>🎯 الوزن المستهدف</h2>
            <input type="number" id="targetWeight" placeholder="أدخل الوزن المستهدف (جرام)" step="0.1">
            <button class="btn" onclick="setTarget()">تعيين الوزن المستهدف</button>
        </div>
        
        <div class="card">
            <h2>📁 إدارة المشاريع</h2>
            <input type="text" id="projectName" placeholder="اسم المشروع الجديد">
            <button class="btn" onclick="createProject()">إنشاء مشروع جديد</button>
            <select id="projectSelect" onchange="loadProject()">
                <option value="">اختر مشروع...</option>
            </select>
        </div>
        
        <div class="card" id="readingsCard" style="display:none;">
            <h2>📊 قراءات المشروع</h2>
            <div class="stats" id="projectStats"></div>
            <table id="readingsTable">
                <thead>
                    <tr>
                        <th>#</th>
                        <th>الوزن المقاس (g)</th>
                        <th>الوزن الحقيقي (g)</th>
                        <th>الفرق (g)</th>
                        <th>التاريخ</th>
                        <th>إجراءات</th>
                    </tr>
                </thead>
                <tbody id="readingsBody"></tbody>
            </table>
        </div>
        
        <div class="card" id="chartCard" style="display:none;">
            <h2>📈 رسم بياني للمقارنة</h2>
            <canvas id="comparisonChart"></canvas>
        </div>
    </div>

    <script>
        let projects = JSON.parse(localStorage.getItem('projects') || '{}');
        let currentProject = null;
        let chart = null;
        
        // تحديث الوزن تلقائياً
        setInterval(updateWeight, 500);
        
        function updateWeight() {
            fetch('/getWeight')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('currentWeight').textContent = data.weight.toFixed(2) + ' g';
                });
        }
        
        function setTarget() {
            const target = document.getElementById('targetWeight').value;
            fetch('/setTarget', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({target: parseFloat(target)})
            }).then(() => alert('تم تعيين الوزن المستهدف!'));
        }
        
        function tareScale() {
            fetch('/tare', {method: 'POST'})
                .then(() => alert('تم إعادة تصفير الميزان!'));
        }
        
        function createProject() {
            const name = document.getElementById('projectName').value.trim();
            if (!name) return alert('الرجاء إدخال اسم المشروع');
            if (projects[name]) return alert('المشروع موجود بالفعل');
            
            projects[name] = {readings: [], created: new Date().toISOString()};
            localStorage.setItem('projects', JSON.stringify(projects));
            updateProjectList();
            document.getElementById('projectName').value = '';
            alert('تم إنشاء المشروع بنجاح!');
        }
        
        function updateProjectList() {
            const select = document.getElementById('projectSelect');
            select.innerHTML = '<option value="">اختر مشروع...</option>';
            Object.keys(projects).forEach(name => {
                const option = document.createElement('option');
                option.value = name;
                option.textContent = name;
                select.appendChild(option);
            });
        }
        
        function loadProject() {
            const name = document.getElementById('projectSelect').value;
            if (!name) {
                document.getElementById('readingsCard').style.display = 'none';
                document.getElementById('chartCard').style.display = 'none';
                return;
            }
            
            currentProject = name;
            displayReadings();
        }
        
        function saveReading() {
            if (!currentProject) return alert('الرجاء اختيار مشروع أولاً');
            
            fetch('/getWeight')
                .then(response => response.json())
                .then(data => {
                    const realWeight = prompt('أدخل الوزن الحقيقي (جرام):');
                    if (realWeight === null) return;
                    
                    const reading = {
                        measured: data.weight,
                        real: parseFloat(realWeight) || 0,
                        timestamp: new Date().toISOString()
                    };
                    
                    projects[currentProject].readings.push(reading);
                    localStorage.setItem('projects', JSON.stringify(projects));
                    displayReadings();
                    alert('تم حفظ القراءة!');
                });
        }
        
        function displayReadings() {
            const project = projects[currentProject];
            if (!project) return;
            
            document.getElementById('readingsCard').style.display = 'block';
            document.getElementById('chartCard').style.display = 'block';
            
            // عرض الإحصائيات
            const readings = project.readings;
            const avgMeasured = readings.reduce((sum, r) => sum + r.measured, 0) / readings.length || 0;
            const avgReal = readings.reduce((sum, r) => sum + r.real, 0) / readings.length || 0;
            const avgError = Math.abs(avgMeasured - avgReal);
            
            document.getElementById('projectStats').innerHTML = `
                <div class="stat-box">
                    <div class="stat-value">${readings.length}</div>
                    <div class="stat-label">عدد القراءات</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value">${avgMeasured.toFixed(2)}</div>
                    <div class="stat-label">متوسط الوزن المقاس</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value">${avgReal.toFixed(2)}</div>
                    <div class="stat-label">متوسط الوزن الحقيقي</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value">${avgError.toFixed(2)}</div>
                    <div class="stat-label">متوسط الخطأ</div>
                </div>
            `;
            
            // عرض الجدول
            const tbody = document.getElementById('readingsBody');
            tbody.innerHTML = readings.map((r, i) => `
                <tr>
                    <td>${i + 1}</td>
                    <td>${r.measured.toFixed(2)}</td>
                    <td>${r.real.toFixed(2)}</td>
                    <td>${(r.measured - r.real).toFixed(2)}</td>
                    <td>${new Date(r.timestamp).toLocaleString('ar-EG')}</td>
                    <td><button class="btn btn-danger" onclick="deleteReading(${i})">حذف</button></td>
                </tr>
            `).join('');
            
            // رسم المخطط
            drawChart();
        }
        
        function deleteReading(index) {
            if (confirm('هل أنت متأكد من حذف هذه القراءة؟')) {
                projects[currentProject].readings.splice(index, 1);
                localStorage.setItem('projects', JSON.stringify(projects));
                displayReadings();
            }
        }
        
        function drawChart() {
            const readings = projects[currentProject].readings;
            const ctx = document.getElementById('comparisonChart').getContext('2d');
            
            if (chart) chart.destroy();
            
            chart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: readings.map((r, i) => `قراءة ${i + 1}`),
                    datasets: [{
                        label: 'الوزن المقاس',
                        data: readings.map(r => r.measured),
                        borderColor: '#667eea',
                        backgroundColor: 'rgba(102, 126, 234, 0.1)',
                        tension: 0.4
                    }, {
                        label: 'الوزن الحقيقي',
                        data: readings.map(r => r.real),
                        borderColor: '#f093fb',
                        backgroundColor: 'rgba(240, 147, 251, 0.1)',
                        tension: 0.4
                    }]
                },
                options: {
                    responsive: true,
                    plugins: {
                        legend: { position: 'top' },
                        title: { display: true, text: 'مقارنة الوزن المقاس والحقيقي' }
                    },
                    scales: {
                        y: { beginAtZero: true, title: { display: true, text: 'الوزن (جرام)' }}
                    }
                }
            });
        }
        
        // تحميل المشاريع عند بدء التشغيل
        updateProjectList();
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleGetWeight() {
  StaticJsonDocument<200> doc;
  doc["weight"] = currentWeight;
  doc["status"] = "ok";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSetTarget() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg("plain"));
    
    targetWeight = doc["target"];
    targetWeightEnabled = true;
    
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }
}

void handleTare() {
  scale.tare();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleCalibrate() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg("plain"));
    
    calibration_factor = doc["factor"];
    scale.set_scale(calibration_factor);
    
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  }
}

void playBuzzer() {
  // صوتين متتاليين
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}