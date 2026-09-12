# Hướng dẫn & Tài liệu Cấu hình API Popup Global

Hệ thống API của Popup Global hoạt động theo cơ chế xác thực gồm 2 tầng:

---

## 1. API Login (`/login/info`)

API này dùng để lấy phiên làm việc (Session `token`) mới từ một token cố định.

- **Endpoint**: `POST https://api-b.popupglobal.ai/login/info`
- **Chỉ cần đúng 3 Headers sau**:

| Header | Mô tả / Giá trị mẫu |
| :--- | :--- |
| `app-id` | `10000001` |
| `device-id` | `087A136A-5BE1-42E4-A785-0BE3B4F831FB` |
| `x-auth-token` | `31rSScyqXLVcT/x6hpFNNu9+egxyAoIJaYyU0oK3RFMnbrYiwxhS8+YsUS4Dm4+WSLI95wDD5Wg18YBEGHCQ22WaWj13kG7bGiJbY1QoT/mkmPbKsaSbgUTNmpsV0YYnJAZ+fIJHgK0=` *(Token cố định dùng để login)* |

### Phản hồi:
- Lấy `token` động tại đường dẫn JSON: `response.data.token`

---

## 2. Các API Khác (ví dụ: `/chat/room/recommend/list`)

Tất cả các API nghiệp vụ sau khi đã có token chỉ cần **4 Headers**:

| Header | Mô tả |
| :--- | :--- |
| `app-id` | `10000001` (dùng chung) |
| `device-id` | `087A136A-5BE1-42E4-A785-0BE3B4F831FB` (dùng chung) |
| `x-auth-token` | `{{x-auth-token}}` (Token động vừa lấy được từ API Login ở Bước 1) |
| `api-sign` | Mã chữ ký số riêng cho từng endpoint |

> [!NOTE]
> **Lưu ý về `api-sign`**: Mỗi endpoint API có một mã `api-sign` riêng biệt:
> - Danh sách phòng gợi ý (`/chat/room/recommend/list`): `670A8BD5BC9600C06EE9ABB71A11EA3DCD1C82B1` hoặc `3B0133ED571B4899E2E2ED948F123ABBC88C1CB8`

---

### 2.2. API Lấy phòng theo Phân loại Thể loại (`/chat/room/classify/code/recommend/list`)

API này dùng để lấy danh sách phòng theo từng thể loại cụ thể (VD: Chit_Chat, Music, Dating...).

- **Method**: `POST`
- **URL**: `https://api-b.popupglobal.ai/chat/room/classify/code/recommend/list`
- **Headers (4 headers)**:
  - `app-id`: `10000001`
  - `device-id`: `087A136A-5BE1-42E4-A785-0BE3B4F831FB`
  - `api-sign`: `F3A52FA676BCF015D25C6B519ABB9B2F30C5D526`
  - `x-auth-token`: `<Token động lấy từ API login>`
  - `content-type`: `application/x-www-form-urlencoded`
- **Body**: `classifyCodeId=33` (hoặc các mã ID thể loại khác)
- **Các trường dữ liệu trích xuất chính**:
  - `signature`: Chữ ký / Bio của chủ phòng / thành viên
  - `userIdEcpt`: Mã ID người dùng đã mã hóa
  - `classifyDTO.classifyCode`: Thể loại phòng (`Chit_Chat`, `Music`, `Dating`...)
  - `roomId`, `topic`, `roomerSize`

---

### 2.3. API Lấy danh sách phòng HOT (`/chat/room/hot/list`)

API này dùng để lấy danh sách các phòng đang thịnh hành (HOT).

- **Method**: `POST`
- **URL**: `https://api-b.popupglobal.ai/chat/room/hot/list`
- **Headers (4 headers)**:
  - `app-id`: `10000001`
  - `device-id`: `087A136A-5BE1-42E4-A785-0BE3B4F831FB`
  - `api-sign`: `A747978795FD1A4C595A9C93AFB03CE684975C99`
  - `x-auth-token`: `<Token động lấy từ API login>`
  - `content-type`: `application/x-www-form-urlencoded`
- **Body**: `classifyCodeId=0`
- **Các trường dữ liệu trích xuất**: `roomId`, `topic`, `classifyDTO.classifyCode`, `signature`, `userIdEcpt`, `roomerSize`

---

### 2.4. API Tham Gia Phòng (`/chat/room/join`)

API này dùng để gửi yêu cầu tham gia (join) vào một phòng cụ thể.

- **Method**: `POST`
- **URL**: `https://api-b.popupglobal.ai/chat/room/join`
- **Headers (4 headers)**:
  - `app-id`: `10000001`
  - `device-id`: `087A136A-5BE1-42E4-A785-0BE3B4F831FB`
  - `api-sign`: `EACF2BD38FF71C959444155A23724536FF7A8D2F`
  - `x-auth-token`: `<Token động lấy từ API login>`
  - `content-type`: `application/x-www-form-urlencoded`
- **Body**: `roomId=<ROOM_ID>&source=14`

---

### 2.5. API Mở Chế Độ PK / Party Model (`/chat/room/apply/party/model`)

API này dùng để gửi yêu cầu mở chế độ PK (Party model) trong phòng khi tham gia.

- **Method**: `POST`
- **URL**: `https://api-b.popupglobal.ai/chat/room/apply/party/model`
- **Headers (4 headers)**:
  - `app-id`: `10000001`
  - `device-id`: `087A136A-5BE1-42E4-A785-0BE3B4F831FB`
  - `api-sign`: `6BF40A58F9ECAC81DC6F4473864868B9DB06D1FB`
  - `x-auth-token`: `<Token động lấy từ API login>`
  - `content-type`: `application/x-www-form-urlencoded`
- **Body**: `playType=1&roomId=<ROOM_ID>`

---

### 2.6. API Thoát Phòng (`/chat/room/exit`)

API này dùng để gửi yêu cầu rời khỏi / thoát phòng sau khi hoàn tất thao tác.

- **Method**: `POST`
- **URL**: `https://api-b.popupglobal.ai/chat/room/exit`
- **Headers (4 headers)**:
  - `app-id`: `10000001`
  - `device-id`: `087A136A-5BE1-42E4-A785-0BE3B4F831FB`
  - `api-sign`: `90000B6D88742B8FD868EC99A3BDE3BF5879E867`
  - `x-auth-token`: `<Token động lấy từ API login>`
  - `content-type`: `application/x-www-form-urlencoded`
- **Body**: `roomId=<ROOM_ID>`

---

## 3. Cơ chế phân trang khi lướt / kéo màn hình (Cursor Pagination)

Hệ thống phân trang của danh sách phòng sử dụng cơ chế **Cursor-based Pagination** thông qua `sortOffset`:

1. **Trang 1 (Lần đầu hoặc Refresh kéo lên đầu)**:
   - Gửi request `POST` không cần body `sortOffset` (hoặc rỗng).
   - Server trả về dữ liệu danh sách phòng kèm theo:
     - `data.hasNextPage`: `true` (nếu còn trang kế tiếp).
     - `data.sortOffset`: ví dụ `"1.789232755266E12"` (dùng làm con trỏ cho trang sau).

2. **Trang 2 trở đi (Khi user cuộn / kéo xuống dưới để tải thêm)**:
   - Thêm header: `'content-type': 'application/x-www-form-urlencoded'`
   - Gửi kèm body dạng form:
     ```http
     sortOffset=1.789232755266E12
     ```
   - Server trả về danh sách phòng của trang tiếp theo và kèm `sortOffset` mới (ví dụ `"1.789239594902E12"`).

3. **Điều kiện dừng**:
   - Khi `data.hasNextPage === false` hoặc không còn `data.sortOffset`.

---

## 4. Luồng hoạt động tự động (Workflow)

```mermaid
sequenceDiagram
    participant App as Ứng Dụng (Node.js)
    participant LoginAPI as API Login (/login/info)
    participant RoomAPI as API Recommend (/chat/room/recommend/list)
    participant JoinAPI as API Join (/chat/room/join)
    participant PkAPI as API PK (/chat/room/apply/party/model)
    participant ExitAPI as API Exit (/chat/room/exit)

    App->>LoginAPI: POST với 3 Headers (app-id, device-id, fixed x-auth-token)
    LoginAPI-->>App: Trả về data.token mới
    
    rect rgb(240, 248, 255)
    Note over App,RoomAPI: Quét danh sách tất cả phòng
    App->>RoomAPI: POST Trang 1..N (Lấy danh sách phòng + Phân loại)
    RoomAPI-->>App: Trả về toàn bộ phòng
    end

    rect rgb(255, 245, 238)
    Note over App,JoinAPI,ExitAPI: Chuỗi tự động từng phòng
    loop Từng phòng
        App->>JoinAPI: POST roomId=...&source=14
        JoinAPI-->>App: Join thành công
        App->>PkAPI: POST playType=1&roomId=...
        PkAPI-->>App: Mở PK thành công
        App->>ExitAPI: POST roomId=...
        ExitAPI-->>App: Thoát phòng thành công
    end
    end
```

---

## 5. Bảng Tra Cứu `api-sign` Cho Từng Endpoint

| Endpoint | `api-sign` | Ghi Chú |
| :--- | :--- | :--- |
| `/login/info` | *(Không cần `api-sign`)* | Chỉ cần 3 headers + Fixed Token |
| `/chat/room/recommend/list` | `3B0133ED571B4899E2E2ED948F123ABBC88C1CB8` | Danh sách phòng gợi ý tổng hợp |
| `/chat/room/classify/code/recommend/list` | `F3A52FA676BCF015D25C6B519ABB9B2F30C5D526` | Danh sách phòng theo thể loại (`classifyCodeId`) |
| `/chat/room/hot/list` | `A747978795FD1A4C595A9C93AFB03CE684975C99` | Danh sách phòng HOT (`classifyCodeId=0`) |
| `/chat/room/join` | `EACF2BD38FF71C959444155A23724536FF7A8D2F` | Tham gia phòng (`roomId`, `source=14`) |
| `/chat/room/apply/party/model` | `6BF40A58F9ECAC81DC6F4473864868B9DB06D1FB` | Mở chế độ PK / Party Model (`playType=1`, `roomId`) |
| `/chat/room/exit` | `90000B6D88742B8FD868EC99A3BDE3BF5879E867` | Thoát khỏi phòng (`roomId`) |

