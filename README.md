# Vector eInk Dashboard

**Installation guide for the Seeed Studio TRMNL 7.5" (OG) DIY Kit**

> **Hướng dẫn cài đặt dành cho bộ Seeed Studio TRMNL 7.5" (OG) DIY Kit**

Vector eInk Dashboard is a standalone dashboard firmware for the XIAO ESP32S3 Plus. It displays time, weather, calendar, regional lunar dates, quotes, cryptocurrency prices, stocks, Todoist tasks, battery level, and uploaded slideshow photos. A NAS or external server is not required for the main dashboard.

> Vector eInk Dashboard là firmware dashboard chạy trực tiếp trên XIAO ESP32S3 Plus. Thiết bị có thể hiển thị giờ, thời tiết, lịch, âm lịch theo vùng, quote, giá tiền điện tử, cổ phiếu, công việc Todoist, pin và ảnh slideshow được tải lên. Dashboard chính không yêu cầu NAS hay máy chủ ngoài.

## 1. Requirements

> ## 1. Chuẩn bị

- Seeed Studio TRMNL 7.5" (OG) DIY Kit.
- A USB-C cable that supports data transfer.
- A Windows, macOS, or Linux computer.
- Arduino IDE 2.x.
- The complete `VectorEInkDashboard` project folder.

> - Bộ Seeed Studio TRMNL 7.5" (OG) DIY Kit.
> - Cáp USB-C có hỗ trợ truyền dữ liệu.
> - Máy tính Windows, macOS hoặc Linux.
> - Arduino IDE 2.x.
> - Toàn bộ thư mục dự án `VectorEInkDashboard`.

Keep the project structure unchanged:

> Giữ nguyên cấu trúc thư mục dự án:

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

The folder name must match the main sketch name: `VectorEInkDashboard`.

> Tên thư mục phải trùng với tên sketch chính: `VectorEInkDashboard`.

## 2. Install Arduino IDE and ESP32 support

> ## 2. Cài Arduino IDE và bộ hỗ trợ ESP32

1. Download and install [Arduino IDE](https://www.arduino.cc/en/software).
2. Open Arduino IDE.
3. Open **File → Preferences** on Windows/Linux or **Arduino IDE → Settings** on macOS.
4. Add this URL to **Additional Boards Manager URLs**:

> 1. Tải và cài [Arduino IDE](https://www.arduino.cc/en/software).
> 2. Mở Arduino IDE.
> 3. Mở **File → Preferences** trên Windows/Linux hoặc **Arduino IDE → Settings** trên macOS.
> 4. Thêm URL sau vào **Additional Boards Manager URLs**:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

5. Open **Boards Manager**.
6. Search for `esp32`.
7. Install **esp32 by Espressif Systems**, version **3.3.5**.
8. Restart Arduino IDE.

> 5. Mở **Boards Manager**.
> 6. Tìm `esp32`.
> 7. Cài **esp32 by Espressif Systems**, phiên bản **3.3.5**.
> 8. Khởi động lại Arduino IDE.

Version 3.3.5 is the version used to build and verify this firmware.

> Phiên bản 3.3.5 là phiên bản đã được dùng để biên dịch và kiểm tra firmware này.

## 3. Install ArduinoJson

> ## 3. Cài ArduinoJson

1. Open **Library Manager**.
2. Search for `ArduinoJson`.
3. Install **ArduinoJson by Benoit Blanchon**, version **7.4.2**.

> 1. Mở **Library Manager**.
> 2. Tìm `ArduinoJson`.
> 3. Cài **ArduinoJson by Benoit Blanchon**, phiên bản **7.4.2**.

`WiFi`, `HTTPClient`, `Preferences`, `WebServer`, `DNSServer`, `FFat`, and `LittleFS` are included with the ESP32 board package. Do not install separate copies.

> Các thư viện `WiFi`, `HTTPClient`, `Preferences`, `WebServer`, `DNSServer`, `FFat` và `LittleFS` đã có sẵn trong gói ESP32. Không cần cài thêm bản riêng.

## 4. Install Seeed_GFX

> ## 4. Cài Seeed_GFX

This project has been verified with **Seeed_GFX 2.0.3**.

> Dự án đã được kiểm tra với **Seeed_GFX 2.0.3**.

1. Open the [Seeed_GFX V2.0.3 release page](https://github.com/Seeed-Studio/Seeed_GFX/releases/tag/V2.0.3).
2. Download the ZIP from **Assets**, or download **Source code (zip)**.
3. Extract it and rename the extracted folder to `Seeed_GFX`.
4. Copy the entire folder into the Arduino libraries directory:

> 1. Mở trang phát hành [Seeed_GFX V2.0.3](https://github.com/Seeed-Studio/Seeed_GFX/releases/tag/V2.0.3).
> 2. Tải file ZIP trong **Assets**, hoặc tải **Source code (zip)**.
> 3. Giải nén và đổi tên thư mục thành `Seeed_GFX`.
> 4. Copy toàn bộ thư mục vào thư mục libraries của Arduino:

| Operating system / Hệ điều hành | Default directory / Thư mục mặc định |
|---|---|
| Windows | `C:\Users\<Username>\Documents\Arduino\libraries\` |
| macOS | `/Users/<Username>/Documents/Arduino/libraries/` |
| Linux | `/home/<Username>/Arduino/libraries/` |

The final path must look like this:

> Đường dẫn sau khi copy phải có dạng:

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

Restart Arduino IDE after copying the library.

> Khởi động lại Arduino IDE sau khi copy thư viện.

### Important: `driver.h`

> ### Quan trọng: `driver.h`

Do not copy `driver.h` into the Seeed_GFX library. It must remain beside `VectorEInkDashboard.ino`. Seeed_GFX automatically reads this hardware profile:

> Không copy `driver.h` vào thư viện Seeed_GFX. File này phải nằm cạnh `VectorEInkDashboard.ino`. Seeed_GFX sẽ tự đọc cấu hình phần cứng:

```cpp
#define BOARD_SCREEN_COMBO 502
#define USE_XIAO_EPAPER_DISPLAY_BOARD_EE04
```

If a separate `TFT_eSPI` library is installed, Arduino may select the wrong library. Check the compilation output. If necessary, temporarily remove the standalone `TFT_eSPI` folder from `Arduino/libraries`, then restart the IDE.

> Nếu máy đã cài riêng thư viện `TFT_eSPI`, Arduino có thể chọn nhầm. Hãy kiểm tra phần Output khi biên dịch. Nếu cần, tạm thời chuyển thư mục `TFT_eSPI` độc lập ra khỏi `Arduino/libraries`, rồi khởi động lại IDE.

## 5. Open the project

> ## 5. Mở dự án

1. Select **File → Open**.
2. Open `VectorEInkDashboard/VectorEInkDashboard.ino`.
3. Confirm that `driver.h`, `icons.h`, and `slideshow_images.h` appear as tabs in Arduino IDE.
4. Do not put Wi‑Fi passwords or API keys in the source code. They are entered later through the setup portal.

> 1. Chọn **File → Open**.
> 2. Mở `VectorEInkDashboard/VectorEInkDashboard.ino`.
> 3. Kiểm tra các tab `driver.h`, `icons.h` và `slideshow_images.h` xuất hiện trong Arduino IDE.
> 4. Không nhập mật khẩu Wi‑Fi hoặc API key vào code. Các thông tin này sẽ được nhập bằng setup portal sau khi nạp.

## 6. Select the board and build options

> ## 6. Chọn board và thông số biên dịch

Connect the kit with a data-capable USB-C cable, then select:

> Kết nối kit bằng cáp USB-C có dữ liệu, sau đó chọn:

```text
Tools → Board → esp32 → XIAO_ESP32S3_PLUS
```

Use these **Tools** settings:

> Sử dụng các thiết lập sau trong menu **Tools**:

| Option / Mục | Value / Giá trị |
|---|---|
| Board | `XIAO_ESP32S3_PLUS` |
| Upload Speed | `460800` or `921600` |
| USB Mode | `Hardware CDC and JTAG` |
| USB CDC On Boot | `Enabled` |
| Upload Mode | `UART0 / Hardware CDC` |
| CPU Frequency | `240MHz (WiFi)` |
| Flash Mode | `QIO 80MHz` |
| Flash Size | `16MB (128Mb)` |
| Partition Scheme | **`16M Flash (3MB APP/9.9MB FATFS)`** |
| PSRAM | `OPI PSRAM` |
| Core Debug Level | `None` |

### The partition setting is mandatory

> ### Bắt buộc chọn đúng partition

The firmware is larger than 2 MB. The default `2MB APP/12.5MB FATFS` partition causes a `Sketch too big` error.

> Firmware lớn hơn 2 MB. Partition mặc định `2MB APP/12.5MB FATFS` sẽ gây lỗi `Sketch too big`.

Always select:

> Luôn chọn:

```text
16M Flash (3MB APP/9.9MB FATFS)
```

Do not select a `No FS` partition. Uploaded slideshow photos require FATFS or LittleFS storage.

> Không chọn partition có chữ `No FS`. Ảnh slideshow tải lên cần vùng lưu trữ FATFS hoặc LittleFS.

### Erase All Flash

> ### Erase All Flash

- For the first installation, or to remove all old settings, set **Erase All Flash Before Sketch Upload → Enabled**.
- After setup, change it back to **Disabled** so later firmware uploads do not erase Wi‑Fi settings, API keys, and photos.

> - Khi cài lần đầu hoặc muốn xóa sạch cấu hình cũ, chọn **Erase All Flash Before Sketch Upload → Enabled**.
> - Sau khi setup xong, đổi lại thành **Disabled** để những lần nạp sau không xóa Wi‑Fi, API key và ảnh.

## 7. Compile and upload

> ## 7. Biên dịch và nạp firmware

1. Select the port named similar to `/dev/cu.usbmodem...` on macOS/Linux or `COM...` on Windows.
2. Do not select Bluetooth speakers, headphones, or ports such as `JBLCharge`.
3. Click **Verify**.
4. When compilation succeeds, click **Upload**.
5. Wait until Arduino IDE reports that the upload is complete.

> 1. Chọn cổng có tên gần giống `/dev/cu.usbmodem...` trên macOS/Linux hoặc `COM...` trên Windows.
> 2. Không chọn cổng Bluetooth, loa, tai nghe hoặc cổng như `JBLCharge`.
> 3. Nhấn **Verify**.
> 4. Khi biên dịch thành công, nhấn **Upload**.
> 5. Chờ Arduino IDE báo nạp hoàn tất.

The verified XIAO ESP32S3 Plus build uses approximately 65% of the 3 MB application partition and 29% of dynamic memory.

> Bản đã kiểm tra cho XIAO ESP32S3 Plus sử dụng khoảng 65% vùng ứng dụng 3 MB và 29% RAM động.

### If uploading fails

> ### Nếu nạp firmware thất bại

1. Close Serial Monitor and any application using the USB port.
2. Disconnect and reconnect the USB cable.
3. Select the newly detected port.
4. Reduce Upload Speed to `460800` or `115200`.
5. If needed, hold **BOOT**, press and release **RESET**, release **BOOT**, and upload again.

> 1. Đóng Serial Monitor và mọi ứng dụng đang sử dụng cổng USB.
> 2. Rút rồi cắm lại cáp USB.
> 3. Chọn lại cổng vừa xuất hiện.
> 4. Giảm Upload Speed xuống `460800` hoặc `115200`.
> 5. Nếu vẫn lỗi, giữ **BOOT**, nhấn rồi thả **RESET**, thả **BOOT**, sau đó upload lại.

## 8. First-time Wi‑Fi setup

> ## 8. Setup Wi‑Fi lần đầu

After the first boot, connect a phone or computer to:

> Sau lần khởi động đầu tiên, dùng điện thoại hoặc máy tính kết nối:

```text
Wi-Fi: Vector-eInk-Setup
Password: vector75
```

The setup portal normally opens automatically. If it does not, open:

> Setup portal thường tự mở. Nếu không, truy cập:

```text
http://192.168.4.1
```

If a phone leaves the setup Wi‑Fi because it has no Internet connection, temporarily disable mobile data.

> Nếu điện thoại tự rời Wi‑Fi setup vì mạng không có Internet, hãy tạm thời tắt dữ liệu di động.

## 9. Configure the portal

> ## 9. Cấu hình trong Portal

### Wi‑Fi

> ### Wi‑Fi

Select or enter a 2.4 GHz Wi‑Fi network and its password.

> Chọn hoặc nhập mạng Wi‑Fi 2.4 GHz và mật khẩu.

### Weather

> ### Thời tiết

- Obtain an API key from [OpenWeather](https://openweathermap.org/appid).
- Select a country and city.
- If the country is not listed, select **Other country**, enter its two-letter ISO country code, and enter the city manually.
- A newly created OpenWeather key may require some time before activation.

> - Lấy API key tại [OpenWeather](https://openweathermap.org/appid).
> - Chọn quốc gia và thành phố.
> - Nếu quốc gia không có trong danh sách, chọn **Other country**, nhập mã ISO quốc gia gồm hai chữ cái rồi nhập thành phố thủ công.
> - API key OpenWeather mới tạo có thể cần một khoảng thời gian để được kích hoạt.

### Optional services

> ### Các dịch vụ tùy chọn

- **CoinMarketCap API key:** cryptocurrency prices. Get a key from the [CoinMarketCap Developer Portal](https://pro.coinmarketcap.com/).
- **Crypto symbols:** keep `BTC,ETH,SOL,DOGE` in the current firmware.
- **ZenQuotes API key:** optional. Quote of the Day works on the free endpoint without a key.
- **Todoist token:** displays up to five tasks due today. Find it under **Todoist → Settings → Integrations → Developer**.
- **Google Apps Script Stock URL:** supplies stock data.

> - **CoinMarketCap API key:** dùng cho giá tiền điện tử. Lấy key tại [CoinMarketCap Developer Portal](https://pro.coinmarketcap.com/).
> - **Crypto symbols:** nên giữ `BTC,ETH,SOL,DOGE` trong phiên bản hiện tại.
> - **ZenQuotes API key:** không bắt buộc. Quote of the Day vẫn hoạt động trên endpoint miễn phí khi để trống.
> - **Todoist token:** hiển thị tối đa năm công việc đến hạn hôm nay. Lấy token tại **Todoist → Settings → Integrations → Developer**.
> - **Google Apps Script Stock URL:** cung cấp dữ liệu cổ phiếu.

The stock endpoint must return:

> Endpoint cổ phiếu phải trả về:

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

The dashboard displays up to five stocks.

> Dashboard hiển thị tối đa năm mã cổ phiếu.

### Time, language, and calendar

> ### Thời gian, ngôn ngữ và lịch

- Select a time zone from `UTC-12` to `UTC+14`. Vietnam uses `UTC+7`.
- Dashboard languages: Auto, Vietnamese, English, Spanish, French, and German.
- Auto language selection follows the selected weather country.
- The Gregorian calendar is always enabled.
- The regional lunar calendar can be enabled for supported countries.

> - Chọn múi giờ từ `UTC-12` đến `UTC+14`. Việt Nam dùng `UTC+7`.
> - Ngôn ngữ dashboard: Auto, Vietnamese, English, Spanish, French và German.
> - Chế độ Auto chọn ngôn ngữ theo quốc gia thời tiết.
> - Lịch dương luôn được bật.
> - Có thể bật âm lịch theo vùng đối với các quốc gia được hỗ trợ.

### Pages and intervals

> ### Trang hiển thị và thời gian chuyển

Available pages:

> Các trang có thể chọn:

- Dashboard.
- Time & Quote.
- Cryptocurrency.
- Stocks.
- Tasks & Calendar.
- Photos.

`Change page` controls page rotation. `Fetch API data` can be set from 1 to 24 hours. `Change photo` controls slideshow timing.

> `Change page` điều khiển thời gian chuyển trang. `Fetch API data` có thể chọn từ 1 đến 24 giờ. `Change photo` điều khiển thời gian đổi ảnh.

Wi‑Fi remains off between scheduled data updates to reduce battery use. Press **Select/Update** to fetch fresh data immediately.

> Wi‑Fi sẽ tắt giữa các lần lấy dữ liệu để tiết kiệm pin. Nhấn **Select/Update** để cập nhật dữ liệu ngay.

### Upload slideshow photos

> ### Tải ảnh slideshow

1. Select one or more JPG or PNG files.
2. The browser crops each photo to 800 × 480 and converts it to a dithered monochrome bitmap.
3. Click **CONVERT & UPLOAD**.
4. Enable **Include photos in the page rotation**.
5. Click **SAVE & REBOOT**.

> 1. Chọn một hoặc nhiều ảnh JPG hoặc PNG.
> 2. Trình duyệt tự crop ảnh thành 800 × 480 và chuyển sang bitmap đen trắng có dithering.
> 3. Nhấn **CONVERT & UPLOAD**.
> 4. Bật **Include photos in the page rotation**.
> 5. Nhấn **SAVE & REBOOT**.

Each converted photo uses 48,000 bytes. The firmware supports up to 50 uploaded photos, subject to available filesystem space.

> Mỗi ảnh đã chuyển đổi chiếm 48.000 byte. Firmware hỗ trợ tối đa 50 ảnh upload, tùy thuộc dung lượng filesystem còn trống.

If the portal reports that photo storage is not mounted, verify the partition setting and use **FORMAT PHOTO STORAGE** once. Formatting removes all uploaded photos.

> Nếu Portal báo photo storage chưa được mount, hãy kiểm tra lại partition rồi dùng **FORMAT PHOTO STORAGE** một lần. Format sẽ xóa toàn bộ ảnh đã upload.

## 10. Device controls

> ## 10. Điều khiển thiết bị

- **Previous:** previous page or photo.
- **Next:** next page or photo.
- **Select/Update:** immediately fetch new API data.

> - **Previous:** trang hoặc ảnh trước.
> - **Next:** trang hoặc ảnh tiếp theo.
> - **Select/Update:** lấy dữ liệu API mới ngay.

To reopen the setup portal:

> Để mở lại setup portal:

1. Quickly press any button five times within approximately 2.5 seconds.
2. A five-second countdown appears.
3. Do not press anything to enter setup mode.
4. Press once during the countdown to cancel.

> 1. Nhấn nhanh bất kỳ nút nào năm lần trong khoảng 2,5 giây.
> 2. Màn hình bắt đầu đếm ngược năm giây.
> 3. Không nhấn thêm để vào chế độ setup.
> 4. Nhấn một lần trong lúc đếm ngược để hủy.

Reconnect to `Vector-eInk-Setup` and open `192.168.4.1`.

> Kết nối lại `Vector-eInk-Setup` rồi mở `192.168.4.1`.

## 11. Troubleshooting

> ## 11. Xử lý lỗi

### `TFT_eSPI.h: No such file or directory`

Seeed_GFX is missing or nested one folder too deep.

> Seeed_GFX chưa được cài hoặc thư mục đang bị lồng thêm một cấp.

Wrong:

> Sai:

```text
Arduino/libraries/Seeed_GFX/Seeed_GFX/TFT_eSPI.h
```

Correct:

> Đúng:

```text
Arduino/libraries/Seeed_GFX/TFT_eSPI.h
```

### Blank screen or display configuration errors

Confirm that `driver.h` is beside the `.ino` file and still contains the `BOARD_SCREEN_COMBO 502` and `USE_XIAO_EPAPER_DISPLAY_BOARD_EE04` definitions.

> Kiểm tra `driver.h` nằm cạnh file `.ino` và vẫn chứa hai cấu hình `BOARD_SCREEN_COMBO 502` và `USE_XIAO_EPAPER_DISPLAY_BOARD_EE04`.

### `Sketch too big`

Select **Partition Scheme → 16M Flash (3MB APP/9.9MB FATFS)**.

> Chọn **Partition Scheme → 16M Flash (3MB APP/9.9MB FATFS)**.

### `Failed to connect to ESP32-S3`

Close Serial Monitor, select the correct USB port, use a data cable, reduce Upload Speed, or enter bootloader mode with BOOT and RESET.

> Đóng Serial Monitor, chọn đúng cổng USB, dùng cáp có dữ liệu, giảm Upload Speed hoặc đưa board vào bootloader bằng BOOT và RESET.

### `Photo storage is not mounted`

Confirm 16 MB flash and the 3 MB APP / 9.9 MB FATFS partition. For a completely fresh installation, enable Erase All Flash once, upload again, and format photo storage from the portal.

> Kiểm tra Flash Size 16 MB và partition 3 MB APP / 9.9 MB FATFS. Với bản cài mới hoàn toàn, bật Erase All Flash một lần, nạp lại rồi format photo storage trong Portal.

### Weather is missing

Verify the OpenWeather key, wait for a new key to activate, use a two-letter country code such as `VN`, and try an English city name such as `Hanoi`.

> Kiểm tra OpenWeather key, chờ key mới được kích hoạt, dùng mã quốc gia hai chữ cái như `VN`, và thử tên thành phố tiếng Anh như `Hanoi`.

### Serial Monitor reports `NTP timeout`

This is not necessarily fatal. The firmware tries NTP first and can use the HTTPS response `Date` header as a fallback clock source.

> Đây không nhất thiết là lỗi nghiêm trọng. Firmware thử NTP trước và có thể dùng trường `Date` trong phản hồi HTTPS làm nguồn thời gian dự phòng.

### The device returns to the setup portal

Check the Wi‑Fi password, make sure a 2.4 GHz network is available, and save the settings again.

> Kiểm tra mật khẩu Wi‑Fi, đảm bảo có mạng 2.4 GHz rồi lưu lại cấu hình.

## 12. Verified configuration

> ## 12. Cấu hình đã kiểm tra

```text
Board: XIAO_ESP32S3_PLUS
ESP32 core: 3.3.5
Seeed_GFX: 2.0.3
ArduinoJson: 7.4.2
Flash: 16 MB
Partition: 3 MB APP / 9.9 MB FATFS
PSRAM: OPI PSRAM
Display: 800 × 480
```

Use these versions for the most reproducible installation. If ESP32 core or Seeed_GFX is upgraded, test compilation, pin mapping, partial refresh, filesystem mounting, and the Wi‑Fi portal before distributing the firmware.

> Nên dùng đúng các phiên bản trên để việc cài đặt ổn định nhất. Nếu nâng ESP32 core hoặc Seeed_GFX, cần kiểm tra lại quá trình biên dịch, pin màn hình, partial refresh, filesystem và Wi‑Fi Portal trước khi phát hành.

## License and credits

> ## Giấy phép và ghi công

This project is licensed under GPL-3.0-or-later. Preserve the license and author credits when modifying or redistributing it.

> Dự án sử dụng giấy phép GPL-3.0-or-later. Hãy giữ nguyên giấy phép và thông tin tác giả khi chỉnh sửa hoặc phân phối lại.

Original FlintOS work: Copyright © 2026 Ameya Angadi.  
Vector eInk Dashboard customization and maintenance: Huy Vector.

> Dự án FlintOS gốc: Copyright © 2026 Ameya Angadi.  
> Tùy biến và duy trì Vector eInk Dashboard: Huy Vector.
