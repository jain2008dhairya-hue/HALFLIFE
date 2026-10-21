# Hướng dẫn cài Vector eInk Dashboard

Hướng dẫn này dành cho **TRMNL 7.5" (OG) DIY Kit của Seeed Studio**, sử dụng **XIAO ESP32S3 Plus** và màn e‑ink 800 × 480.

## 1. Chuẩn bị

Bạn cần:

- TRMNL 7.5" (OG) DIY Kit.
- Cáp USB-C **có truyền dữ liệu**.
- Máy Windows, macOS hoặc Linux.
- Arduino IDE 2.x.
- Toàn bộ thư mục dự án `VectorEInkDashboard`.

Không tách riêng file `.ino`. Cấu trúc tối thiểu phải giữ nguyên:

```text
VectorEInkDashboard/
├── VectorEInkDashboard.ino
├── driver.h
├── icons.h
├── slideshow_images.h
└── fonts/
    ├── LunarText18.h
    ├── NotoSansBold56.h
    ├── NotoSansMedium18.h
    ├── NotoSansMedium24.h
    └── NotoSansSemiBold32.h
```

Tên thư mục `VectorEInkDashboard` phải trùng với tên file `VectorEInkDashboard.ino`.

## 2. Cài Arduino IDE

1. Tải và cài [Arduino IDE](https://www.arduino.cc/en/software).
2. Mở Arduino IDE.
3. Vào:
   - Windows/Linux: **File → Preferences**.
   - macOS: **Arduino IDE → Settings**.
4. Tại **Additional Boards Manager URLs**, thêm:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

5. Mở **Boards Manager** ở thanh bên trái.
6. Tìm `esp32`.
7. Cài gói **esp32 by Espressif Systems** phiên bản **3.3.5**.
8. Khởi động lại Arduino IDE.

Phiên bản 3.3.5 là phiên bản đã được dùng để biên dịch và kiểm tra bản firmware này.

## 3. Cài ArduinoJson

1. Mở **Library Manager**.
2. Tìm `ArduinoJson`.
3. Cài **ArduinoJson by Benoit Blanchon**, phiên bản **7.4.2**.

Các thư viện `WiFi`, `HTTPClient`, `Preferences`, `WebServer`, `DNSServer`, `FFat` và `LittleFS` đã có sẵn trong gói ESP32, không cần cài riêng.

## 4. Cài Seeed_GFX

Bản đã được kiểm tra với dự án là **Seeed_GFX 2.0.3**.

1. Mở trang [Seeed_GFX V2.0.3](https://github.com/Seeed-Studio/Seeed_GFX/releases/tag/V2.0.3).
2. Tải file ZIP trong phần **Assets** hoặc chọn **Source code (zip)**.
3. Giải nén file vừa tải.
4. Đổi tên thư mục đã giải nén thành:

```text
Seeed_GFX
```

5. Copy toàn bộ thư mục `Seeed_GFX` vào thư mục thư viện Arduino:

| Hệ điều hành | Thư mục mặc định |
|---|---|
| Windows | `C:\Users\<Tên người dùng>\Documents\Arduino\libraries\` |
| macOS | `/Users/<Tên người dùng>/Documents/Arduino/libraries/` |
| Linux | `/home/<Tên người dùng>/Arduino/libraries/` |

Kết quả phải giống:

```text
Arduino/
└── libraries/
    └── Seeed_GFX/
        ├── Extensions/
        ├── TFT_Drivers/
        ├── User_Setups/
        ├── TFT_eSPI.cpp
        ├── TFT_eSPI.h
        └── library.properties
```

6. Khởi động lại Arduino IDE.

### Lưu ý về `driver.h`

Không copy `driver.h` vào thư mục `Seeed_GFX`. File này phải nằm cạnh file `.ino` trong thư mục dự án. Seeed_GFX sẽ tự đọc cấu hình:

```cpp
#define BOARD_SCREEN_COMBO 502
#define USE_XIAO_EPAPER_DISPLAY_BOARD_EE04
```

Nếu máy đã cài riêng thư viện `TFT_eSPI`, Arduino có thể chọn nhầm thư viện. Khi biên dịch, hãy kiểm tra phần Output. Nếu IDE không dùng `Seeed_GFX`, tạm thời xóa hoặc di chuyển thư mục `TFT_eSPI` độc lập ra khỏi `Arduino/libraries`, sau đó khởi động lại IDE.

## 5. Mở dự án

1. Chọn **File → Open**.
2. Mở:

```text
VectorEInkDashboard/VectorEInkDashboard.ino
```

3. Kiểm tra các tab `driver.h`, `icons.h`, `slideshow_images.h` xuất hiện trong Arduino IDE.
4. Không cần nhập Wi‑Fi hoặc API key vào code. Tất cả sẽ được cấu hình bằng Wi‑Fi Portal sau khi nạp.

## 6. Chọn board và cấu hình

Kết nối kit với máy tính bằng cáp USB-C, sau đó chọn:

```text
Tools → Board → esp32 → XIAO_ESP32S3_PLUS
```

Thiết lập các mục trong menu **Tools** như sau:

| Mục | Giá trị |
|---|---|
| Board | `XIAO_ESP32S3_PLUS` |
| Upload Speed | `460800` hoặc `921600` |
| USB Mode | `Hardware CDC and JTAG` |
| USB CDC On Boot | `Enabled` |
| Upload Mode | `UART0 / Hardware CDC` |
| CPU Frequency | `240MHz (WiFi)` |
| Flash Mode | `QIO 80MHz` |
| Flash Size | `16MB (128Mb)` |
| Partition Scheme | **`16M Flash (3MB APP/9.9MB FATFS)`** |
| PSRAM | `OPI PSRAM` |
| Core Debug Level | `None` |

### Partition bắt buộc

Firmware hiện lớn hơn 2 MB. Nếu chọn `16M Flash (2MB APP/12.5MB FATFS)`, Arduino sẽ báo:

```text
Sketch too big
```

Phải chọn:

```text
16M Flash (3MB APP/9.9MB FATFS)
```

Partition này cũng cung cấp FATFS để lưu ảnh slideshow được upload từ Portal. Không chọn `Maximum APP ... No FS`, vì chế độ đó không có vùng lưu ảnh.

### Erase All Flash

- Lần nạp đầu tiên hoặc khi muốn xóa sạch cấu hình cũ: chọn **Enabled**.
- Sau khi thiết bị đã được setup và có ảnh: đổi lại **Disabled** để các lần nạp sau không xóa Wi‑Fi, API key và ảnh đã upload.

## 7. Biên dịch và nạp firmware

1. Chọn đúng cổng có tên gần giống:

```text
/dev/cu.usbmodem...
```

trên macOS/Linux, hoặc:

```text
COM...
```

trên Windows.

Không chọn cổng Bluetooth, loa, tai nghe hoặc cổng có tên như `JBLCharge`.

2. Nhấn **Verify** để biên dịch.
3. Nếu biên dịch thành công, nhấn **Upload**.
4. Chờ đến khi IDE báo hoàn tất.

Với cấu hình đã kiểm tra, firmware sử dụng khoảng:

```text
Program storage: 65% của vùng APP 3 MB
Dynamic memory: 29%
```

### Nếu không kết nối được khi upload

1. Đóng **Serial Monitor** và mọi ứng dụng đang dùng cổng USB.
2. Rút và cắm lại cáp.
3. Chọn lại cổng mới xuất hiện.
4. Giảm **Upload Speed** xuống `460800` hoặc `115200`.
5. Nếu vẫn không được, giữ nút **BOOT**, nhấn rồi thả **RESET**, sau đó thả **BOOT** và upload lại.

## 8. Setup lần đầu bằng Wi‑Fi Portal

Sau khi nạp xong, màn hình sẽ hiện thông tin setup.

1. Dùng điện thoại hoặc máy tính kết nối Wi‑Fi:

```text
Wi-Fi: Vector-eInk-Setup
Password: vector75
```

2. Nếu Portal không tự mở, truy cập:

```text
http://192.168.4.1
```

3. Nếu điện thoại tự chuyển về 4G/5G vì Wi‑Fi “không có Internet”, tạm thời tắt dữ liệu di động.

## 9. Điền thông tin trong Portal

### Wi‑Fi

- Chọn hoặc nhập tên Wi‑Fi.
- Nhập mật khẩu.
- Nên dùng Wi‑Fi 2.4 GHz.

### Weather

- `OpenWeather API key`: lấy tại [OpenWeather](https://openweathermap.org/appid).
- Chọn quốc gia và thành phố.
- Nếu không có trong danh sách, chọn **Other country**, nhập mã quốc gia ISO gồm hai chữ cái, rồi nhập thành phố thủ công.
- API key OpenWeather mới tạo có thể cần một khoảng thời gian để được kích hoạt.

### Services

Các mục này là tùy chọn. Có thể để trống nếu không dùng trang tương ứng.

- `CoinMarketCap API key`: dùng cho trang Cryptocurrency; lấy tại [CoinMarketCap Developer Portal](https://pro.coinmarketcap.com/).
- `Crypto symbols`: nên giữ `BTC,ETH,SOL,DOGE` ở phiên bản hiện tại.
- `ZenQuotes API key`: không bắt buộc; endpoint Quote of the Day miễn phí vẫn hoạt động khi để trống.
- `Todoist token`: dùng cho tối đa 5 công việc đến hạn hôm nay; lấy trong **Todoist → Settings → Integrations → Developer**.
- `Google Apps Script Stock URL`: URL web app trả JSON theo dạng:

```json
{
  "stocks": [
    {
      "symbol": "AAPL",
      "name": "Apple Inc.",
      "price": "214.40",
      "change": "+1.2%"
    }
  ]
}
```

Thiết bị hiển thị tối đa 5 mã cổ phiếu.

### Time & language

- Chọn múi giờ theo dạng `UTC-12` đến `UTC+14`.
- Việt Nam chọn `UTC+7`.
- `Dashboard language` hỗ trợ Auto, Vietnamese, English, Spanish, French và German.
- Ở chế độ Auto, ngôn ngữ được chọn theo quốc gia thời tiết.

### Calendar type

- Lịch dương luôn được bật.
- Có thể bật lịch âm theo vùng hỗ trợ.
- Nếu quốc gia không được hỗ trợ, ô lịch âm sẽ bị vô hiệu hóa.

### Display pages

Chọn các trang muốn đưa vào vòng quay:

- Dashboard.
- Time & Quote.
- Cryptocurrency.
- Stocks.
- Tasks & Calendar.
- Photos.

Nếu bỏ chọn hết, Dashboard sẽ tự được bật để thiết bị luôn có trang hiển thị.

### Intervals

- `Change page`: thời gian chuyển giữa các trang.
- `Fetch API data`: từ 1 đến 24 giờ.
- `Change photo`: thời gian chuyển ảnh.

Wi‑Fi được tắt giữa các lần lấy dữ liệu để tiết kiệm pin. Nhấn nút **Select/Update** trên thiết bị để lấy dữ liệu ngay.

### Slideshow photos

1. Chọn ảnh JPG hoặc PNG.
2. Portal tự crop thành 800 × 480 và chuyển sang bitmap đen trắng có dithering.
3. Nhấn **CONVERT & UPLOAD**.
4. Nếu báo storage chưa mount, kiểm tra lại Partition Scheme. Sau đó dùng **FORMAT PHOTO STORAGE** một lần.
5. Bật **Include photos in the page rotation**.
6. Nhấn **SAVE & REBOOT**.

Mỗi ảnh sau khi chuyển đổi chiếm 48.000 byte. Firmware giới hạn tối đa 50 ảnh upload; dung lượng thực tế còn phụ thuộc partition.

## 10. Điều khiển thiết bị

- **Previous**: trang hoặc ảnh trước.
- **Next**: trang hoặc ảnh tiếp theo.
- **Select/Update**: kết nối Wi‑Fi và cập nhật dữ liệu API ngay.

Để mở lại Wi‑Fi Portal:

1. Nhấn nhanh tổng cộng 5 lần vào bất kỳ nút nào trong khoảng 2,5 giây.
2. Màn hình đếm ngược từ 5 giây.
3. Không nhấn thêm để thiết bị mở Portal.
4. Nhấn một lần trong lúc đếm ngược để hủy.

Sau đó kết nối lại `Vector-eInk-Setup` và mở `192.168.4.1`.

## 11. Lỗi thường gặp

### `TFT_eSPI.h: No such file or directory`

Seeed_GFX chưa được cài đúng hoặc thư mục bị lồng hai lớp, ví dụ:

```text
Arduino/libraries/Seeed_GFX/Seeed_GFX/TFT_eSPI.h
```

Hãy sửa thành:

```text
Arduino/libraries/Seeed_GFX/TFT_eSPI.h
```

### Lỗi cấu hình màn hình hoặc màn trắng

Kiểm tra `driver.h` còn nằm cạnh `.ino` và chứa:

```cpp
#define BOARD_SCREEN_COMBO 502
#define USE_XIAO_EPAPER_DISPLAY_BOARD_EE04
```

### `Sketch too big`

Chọn đúng:

```text
Partition Scheme → 16M Flash (3MB APP/9.9MB FATFS)
```

### `Failed to connect to ESP32-S3`

- Chọn đúng cổng `usbmodem`/COM.
- Đóng Serial Monitor.
- Dùng cáp USB có dữ liệu.
- Giảm Upload Speed.
- Đưa board vào bootloader bằng BOOT + RESET.

### `Photo storage is not mounted`

- Kiểm tra Flash Size là 16 MB.
- Kiểm tra partition có FATFS.
- Bật `Erase All Flash`, nạp lại một lần, sau đó chuyển về Disabled.
- Mở Portal và format photo storage.

Việc format hoặc bật `Erase All Flash` sẽ xóa toàn bộ ảnh và cấu hình đã lưu.

### Thời tiết không hiện

- Kiểm tra OpenWeather API key.
- Chờ key mới được kích hoạt.
- Kiểm tra mã quốc gia gồm đúng hai chữ cái, ví dụ `VN`.
- Thử tên thành phố tiếng Anh, ví dụ `Hanoi` hoặc `Ho Chi Minh City`.
- Nhấn Select/Update để lấy dữ liệu ngay.

### Serial báo `NTP timeout`

Không nhất thiết là lỗi nghiêm trọng. Firmware sẽ thử NTP trước, sau đó có thể lấy thời gian dự phòng từ HTTP `Date` của phản hồi HTTPS và tiếp tục chạy.

### Thiết bị quay lại Portal

Thường do:

- Sai mật khẩu Wi‑Fi.
- Wi‑Fi chỉ phát 5 GHz.
- Thiết bị không thể kết nối mạng trong thời gian cho phép.

Kiểm tra lại Wi‑Fi trong Portal rồi lưu và khởi động lại.

## 12. Cấu hình đã kiểm tra

```text
Board: XIAO_ESP32S3_PLUS
ESP32 core: 3.3.5
Seeed_GFX: 2.0.3
ArduinoJson: 7.4.2
Flash: 16 MB
Partition: 3 MB APP / 9.9 MB FATFS
PSRAM: OPI PSRAM
Resolution: 800 × 480
```

Nên dùng đúng các phiên bản trên để có kết quả giống bản đã kiểm tra. Khi nâng ESP32 core hoặc Seeed_GFX, hãy biên dịch và kiểm tra lại partial refresh, pin mapping, filesystem và Wi‑Fi Portal trước khi phát hành.
