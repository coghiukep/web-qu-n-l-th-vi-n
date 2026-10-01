# Web quản lý thư viện (C++17, Crow, SQLite)

Mượn sách là trọng tâm: chọn chủ đề → nhận gợi ý (chủ đề + lịch sử mượn + độ phổ biến).
Đăng ký chỉ cần email + mật khẩu; ai cũng thêm sách (kèm ảnh bìa) được. Có tài khoản (khách / thủ thư), giỏ hàng + thanh toán giả lập, và bộ test GoogleTest.

## Cài đặt (Ubuntu / WSL)
    sudo apt install build-essential cmake libsqlite3-dev libasio-dev
    # macOS: brew install cmake sqlite asio

## Build và chạy
    cmake -S . -B build && cmake --build build -j
    ./build/library_server          # chạy từ thư mục gốc dự án, mở http://localhost:18080
Thủ thư mặc định: `admin@thuvien.local` / `admin12345` (đổi trước khi dùng thật).

## Chạy test
    ./build/library_tests           # hoặc: ctest --test-dir build --output-on-failure
Chỉ build phần lõi + test (không cần Crow): `cmake -S . -B build -DBUILD_SERVER=OFF`

## Cấu trúc
- `src/database.*`    bọc SQLite (Stmt, Tx)
- `src/auth.*`        đăng ký, đăng nhập, session, băm mật khẩu
- `src/library.*`     mượn/trả, phí trễ, giỏ hàng, thanh toán
- `src/recommender.*` thuật toán gợi ý (hàm thuần `rankBooks`)
- `src/main.cpp`      các route Crow (JSON API) + dữ liệu mẫu
- `static/index.html` giao diện
- `tests/`            unit test

## Luật nghiệp vụ (để viết test)
- Mượn tối đa 5 cuốn cùng lúc, 14 ngày; không mượn trùng cuốn đang mượn; có sách quá hạn thì không mượn thêm.
- Phí trễ: 2.000đ cho mỗi ngày bắt đầu trễ. Trả đúng hạn thì miễn phí.
- Điểm gợi ý = 0.5·chủ đề + 0.3·lịch sử + 0.2·phổ biến; bỏ sách đã mượn và sách hết bản.
- Thanh toán `"payment":"declined"` mô phỏng bị từ chối, không thay đổi kho/giỏ.

## API
POST /api/register, /api/login, /api/logout · GET /api/me, /api/topics, /api/books?topic=&q=
GET /api/recommendations?topic=&limit= · POST /api/borrow, /api/return · GET /api/loans
GET/POST /api/cart · POST /api/cart/clear (hủy đơn) · POST /api/checkout · POST /api/books (mọi người dùng đã đăng nhập, `image_url` tùy chọn) · (thủ thư) GET /api/admin/loans
Gửi token qua header `Authorization: Bearer <token>`.
