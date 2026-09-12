/**
 * Popup Global Room Scanner & Category Aggregator Tool
 * - Bước 1: Login lấy token mới
 * - Bước 2: Quét tất cả phòng từ Recommend List (hết tất cả trang sortOffset)
 * - Bước 3: Quét danh sách phòng HOT
 * - Bước 4: Quét danh sách phòng theo Classify
 * - Bước 5: Gộp, loại bỏ trùng lặp và Phân loại theo Thể loại (Chit_Chat, Music, Dating, Game...)
 * - Bước 6: In bảng tổng kết và xuất ra file rooms_categorized.json
 */

import fs from 'fs';

const BASE_CONFIG = {
  appId: '10000001',
  deviceId: '087A136A-5BE1-42E4-A785-0BE3B4F831FB',
  // Token cố định dùng cho API login
  fixedLoginToken: '31rSScyqXLVcT/x6hpFNNu9+egxyAoIJaYyU0oK3RFMnbrYiwxhS8+YsUS4Dm4+WSLI95wDD5Wg18YBEGHCQ22WaWj13kG7bGiJbY1QoT/mkmPbKsaSbgUTNmpsV0YYnJAZ+fIJHgK0='
};

/**
 * 1. API Login - Lấy Session Token mới (Chỉ cần 3 headers)
 */
async function loginAndGetToken() {
  const url = 'https://api-b.popupglobal.ai/login/info';
  console.log('[1] Đang gọi API Login để lấy Token mới...');

  try {
    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'app-id': BASE_CONFIG.appId,
        'device-id': BASE_CONFIG.deviceId,
        'x-auth-token': BASE_CONFIG.fixedLoginToken
      }
    });

    const json = await response.json();
    const token = json?.data?.token;

    if (!token) {
      throw new Error('Không lấy được token từ API Login: ' + JSON.stringify(json));
    }

    console.log('[+] Đăng nhập thành công! Token mới:', token.substring(0, 40) + '...\n');
    return token;
  } catch (error) {
    console.error('[-] Lỗi Login:', error.message);
    throw error;
  }
}

/**
 * Helper: Trích xuất thông tin chuẩn từ raw room object
 */
function parseRoomData(room, source = 'recommend') {
  const masterUser = room.roomerModelList?.find(u => u.role === 'MASTER') || room.roomerModelList?.[0] || {};
  const signature = masterUser.signature || '(Trống)';
  const userIdEcpt = masterUser.userIdEcpt || room.ownerIdIdEcpt || '';
  const classifyCode = room.classifyDTO?.classifyCode || 'Unknown';

  const members = (room.roomerModelList || []).map(m => ({
    userId: m.userId,
    userIdEcpt: m.userIdEcpt,
    role: m.role,
    signature: m.signature || '(Trống)',
    gender: m.gender === 1 ? 'Nam' : (m.gender === 2 ? 'Nữ' : 'Khác')
  }));

  return {
    roomId: room.roomId,
    topic: room.topic || '(Không tên)',
    classifyCode: classifyCode,
    classifyLangKey: room.classifyCodeLangKey || '',
    source: source,
    ownerId: room.ownerId,
    ownerIdIdEcpt: room.ownerIdIdEcpt || userIdEcpt,
    userIdEcpt: userIdEcpt || room.ownerIdIdEcpt || '',
    signature: signature,
    roomerSize: room.roomerSize || 0,
    members: members
  };
}

/**
 * 2. API Lấy danh sách phòng gợi ý (Có hỗ trợ phân trang bằng sortOffset)
 */
async function getRecommendRoomPage(authToken, sortOffset = null) {
  const url = 'https://api-b.popupglobal.ai/chat/room/recommend/list';
  const apiSign = '3B0133ED571B4899E2E2ED948F123ABBC88C1CB8';

  const headers = {
    'app-id': BASE_CONFIG.appId,
    'device-id': BASE_CONFIG.deviceId,
    'api-sign': apiSign,
    'x-auth-token': authToken
  };

  let body = undefined;
  if (sortOffset) {
    headers['content-type'] = 'application/x-www-form-urlencoded';
    body = `sortOffset=${encodeURIComponent(sortOffset)}`;
  }

  try {
    const response = await fetch(url, {
      method: 'POST',
      headers: headers,
      body: body
    });

    const json = await response.json();
    const data = json?.data || {};
    const roomList = data.list || data.recommendList || [];
    const nextSortOffset = data.sortOffset;
    const hasNextPage = Boolean(data.hasNextPage);

    return { roomList, nextSortOffset, hasNextPage };
  } catch (error) {
    console.error('[-] Lỗi lấy Recommend room page:', error.message);
    return { roomList: [], nextSortOffset: null, hasNextPage: false };
  }
}

/**
 * Quét toàn bộ các trang Recommend rooms
 */
async function scanAllRecommendRooms(authToken, maxPages = 100) {
  const rooms = [];
  const visitedOffsets = new Set();
  let currentSortOffset = null;
  let page = 1;

  console.log(`[2] Đang quét tất cả các trang Recommend rooms...`);

  while (page <= maxPages) {
    const result = await getRecommendRoomPage(authToken, currentSortOffset);
    const { roomList, nextSortOffset, hasNextPage } = result;

    if (!roomList || roomList.length === 0) break;

    process.stdout.write(`    -> Đã tải Trang ${page} (${roomList.length} phòng)...\r`);
    roomList.forEach(r => rooms.push(parseRoomData(r, 'recommend')));

    if (!hasNextPage || !nextSortOffset || visitedOffsets.has(nextSortOffset)) break;

    visitedOffsets.add(nextSortOffset);
    currentSortOffset = nextSortOffset;
    page++;

    await new Promise(r => setTimeout(r, 200));
  }

  console.log(`\n[+] Hoàn tất Recommend: ${page} trang, thu thập được ${rooms.length} lượt phòng.`);
  return rooms;
}

/**
 * 3. API Lấy danh sách phòng HOT
 */
async function scanHotRooms(authToken) {
  const url = 'https://api-b.popupglobal.ai/chat/room/hot/list';
  const apiSign = 'A747978795FD1A4C595A9C93AFB03CE684975C99';

  console.log('\n[3] Đang quét danh sách phòng HOT...');
  try {
    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'app-id': BASE_CONFIG.appId,
        'device-id': BASE_CONFIG.deviceId,
        'api-sign': apiSign,
        'x-auth-token': authToken,
        'content-type': 'application/x-www-form-urlencoded'
      },
      body: 'classifyCodeId=0'
    });

    const json = await response.json();
    const list = json?.data?.list || json?.data?.roomList || [];
    console.log(`[+] Hoàn tất Hot Rooms: thu thập được ${list.length} phòng.`);
    return list.map(r => parseRoomData(r, 'hot'));
  } catch (error) {
    console.error('[-] Lỗi quét Hot rooms:', error.message);
    return [];
  }
}

/**
 * 4. API Lấy danh sách phòng Classify
 */
async function scanClassifyRooms(authToken, classifyCodeIds = [0, 33]) {
  const url = 'https://api-b.popupglobal.ai/chat/room/classify/code/recommend/list';
  const apiSign = 'F3A52FA676BCF015D25C6B519ABB9B2F30C5D526';
  const allClassifyRooms = [];

  console.log('\n[4] Đang quét danh sách phòng theo Category Classify...');
  for (const codeId of classifyCodeIds) {
    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'app-id': BASE_CONFIG.appId,
          'device-id': BASE_CONFIG.deviceId,
          'api-sign': apiSign,
          'x-auth-token': authToken,
          'content-type': 'application/x-www-form-urlencoded'
        },
        body: `classifyCodeId=${codeId}`
      });

      const json = await response.json();
      const list = json?.data?.list || [];
      console.log(`    - Classify ID ${codeId}: tải được ${list.length} phòng.`);
      list.forEach(r => allClassifyRooms.push(parseRoomData(r, `classify_${codeId}`)));
    } catch (error) {
      console.error(`[-] Lỗi quét Classify ID ${codeId}:`, error.message);
    }
  }

  return allClassifyRooms;
}

/**
 * 5. Tổng hợp, Phân loại và Hiển thị Báo cáo
 */
function aggregateAndDisplayCategories(allRoomList) {
  // Loại bỏ phòng trùng lặp theo roomId
  const uniqueRoomsMap = new Map();
  allRoomList.forEach(room => {
    if (!uniqueRoomsMap.has(room.roomId)) {
      uniqueRoomsMap.set(room.roomId, room);
    } else {
      // Nếu đã có, giữ lại hoặc cập nhật nếu phòng có phân loại rõ hơn
      const existing = uniqueRoomsMap.get(room.roomId);
      if (existing.classifyCode === 'Unknown' && room.classifyCode !== 'Unknown') {
        uniqueRoomsMap.set(room.roomId, room);
      }
    }
  });

  const uniqueRooms = Array.from(uniqueRoomsMap.values());

  // Phân nhóm theo Thể loại (classifyCode)
  const categoryGroups = {};
  uniqueRooms.forEach(room => {
    const cat = room.classifyCode || 'Chưa_Phân_Loại';
    if (!categoryGroups[cat]) {
      categoryGroups[cat] = [];
    }
    categoryGroups[cat].push(room);
  });

  console.log('\n========================================================================');
  console.log('                 📊 BẢNG TỔNG HỢP SỐ LƯỢNG PHÒNG THEO THỂ LOẠI');
  console.log('========================================================================');

  const summaryTable = Object.keys(categoryGroups).map((cat, i) => ({
    STT: i + 1,
    'Thể Loại (Category)': cat,
    'Số Lượng Phòng': categoryGroups[cat].length,
    'Tỷ Lệ (%)': ((categoryGroups[cat].length / uniqueRooms.length) * 100).toFixed(1) + '%'
  }));

  console.table(summaryTable);
  console.log(`[+] TỔNG SỐ PHÒNG DUY NHẤT (UNIQUE): ${uniqueRooms.length} phòng\n`);

  // Hiển thị từng danh mục
  for (const [catName, rooms] of Object.entries(categoryGroups)) {
    console.log(`\n================== 📁 THỂ LOẠI: ${catName.toUpperCase()} (${rooms.length} phòng) ==================`);
    console.table(rooms.map((r, index) => ({
      STT: index + 1,
      'Room ID': r.roomId,
      'Tên Phòng (Topic)': r.topic,
      'Số Người': r.roomerSize,
      'Signature (Chủ Phòng)': r.signature,
      'UserIdEcpt': r.userIdEcpt
    })));
  }

  // Xuất file JSON phân loại
  const exportData = {
    totalUniqueRooms: uniqueRooms.length,
    scannedAt: new Date().toISOString(),
    categories: categoryGroups
  };

  fs.writeFileSync('rooms_categorized.json', JSON.stringify(exportData, null, 2), 'utf-8');
  console.log('\n[✔] Đã lưu kết quả phân loại đầy đủ vào file: rooms_categorized.json\n');

  return { uniqueRooms, categoryGroups };
}

/**
 * 6. API Tham gia phòng (chat/room/join)
 * @param {string} authToken
 * @param {string} roomId
 * @param {string|number} source Mặc định là 14
 */
async function joinRoom(authToken, roomId, source = '14') {
  const url = 'https://api-b.popupglobal.ai/chat/room/join';
  const apiSign = 'EACF2BD38FF71C959444155A23724536FF7A8D2F';

  const headers = {
    'app-id': BASE_CONFIG.appId,
    'device-id': BASE_CONFIG.deviceId,
    'api-sign': apiSign,
    'x-auth-token': authToken,
    'content-type': 'application/x-www-form-urlencoded'
  };

  const body = `roomId=${encodeURIComponent(roomId)}&source=${encodeURIComponent(source)}`;

  try {
    const response = await fetch(url, {
      method: 'POST',
      headers: headers,
      body: body
    });

    const json = await response.json();
    const success = json?.code === 10001 || json?.success === true;
    return {
      roomId,
      success,
      code: json?.code,
      message: json?.message,
      data: json?.data
    };
  } catch (error) {
    return {
      roomId,
      success: false,
      error: error.message
    };
  }
}

/**
 * 7. API Mở chế độ PK / Party Model (chat/room/apply/party/model)
 * @param {string} authToken
 * @param {string} roomId
 * @param {number|string} playType 1 = Chế độ PK
 */
async function applyPartyModel(authToken, roomId, playType = 1) {
  const url = 'https://api-b.popupglobal.ai/chat/room/apply/party/model';
  const apiSign = '6BF40A58F9ECAC81DC6F4473864868B9DB06D1FB';

  const headers = {
    'app-id': BASE_CONFIG.appId,
    'device-id': BASE_CONFIG.deviceId,
    'api-sign': apiSign,
    'x-auth-token': authToken,
    'content-type': 'application/x-www-form-urlencoded'
  };

  const body = `playType=${encodeURIComponent(playType)}&roomId=${encodeURIComponent(roomId)}`;

  try {
    const response = await fetch(url, {
      method: 'POST',
      headers: headers,
      body: body
    });

    const json = await response.json();
    const success = json?.code === 10001 || json?.success === true;
    return {
      roomId,
      success,
      code: json?.code,
      message: json?.message,
      data: json?.data
    };
  } catch (error) {
    return {
      roomId,
      success: false,
      error: error.message
    };
  }
}

/**
 * 8. API Thoát phòng (chat/room/exit)
 * @param {string} authToken
 * @param {string} roomId
 */
async function exitRoom(authToken, roomId) {
  const url = 'https://api-b.popupglobal.ai/chat/room/exit';
  const apiSign = '90000B6D88742B8FD868EC99A3BDE3BF5879E867';

  const headers = {
    'app-id': BASE_CONFIG.appId,
    'device-id': BASE_CONFIG.deviceId,
    'api-sign': apiSign,
    'x-auth-token': authToken,
    'content-type': 'application/x-www-form-urlencoded'
  };

  const body = `roomId=${encodeURIComponent(roomId)}`;

  try {
    const response = await fetch(url, {
      method: 'POST',
      headers: headers,
      body: body
    });

    const json = await response.json();
    const success = json?.code === 10001 || json?.success === true;
    return {
      roomId,
      success,
      code: json?.code,
      message: json?.message,
      data: json?.data
    };
  } catch (error) {
    return {
      roomId,
      success: false,
      error: error.message
    };
  }
}

/**
 * Tự động thực hiện chuỗi: Vào phòng (Join) -> Mở PK (playType=1) -> Thoát phòng (Exit)
 */
async function processRoomPipeline(authToken, roomList, delayMs = 300) {
  console.log(`\n========================================================================================`);
  console.log(`     🚀 BẮT ĐẦU CHUỖI TỰ ĐỘNG: [JOIN PHÒNG ➔ MỞ PK ➔ THOÁT PHÒNG] (${roomList.length} PHÒNG)`);
  console.log(`========================================================================================`);

  const results = [];

  for (let i = 0; i < roomList.length; i++) {
    const room = roomList[i];
    
    // Bước 1: Gửi request Join phòng
    const joinRes = await joinRoom(authToken, room.roomId);
    
    // Bước 2: Gửi request Mở PK (playType=1)
    const pkRes = await applyPartyModel(authToken, room.roomId, 1);

    // Bước 3: Gửi request Thoát phòng
    const exitRes = await exitRoom(authToken, room.roomId);

    const joinIcon = joinRes.success ? '✅' : '❌';
    const pkIcon = pkRes.success ? '⚔️ PK ON' : `⚠️ PK (${pkRes.message || pkRes.code || 'Fail'})`;
    const exitIcon = exitRes.success ? '🚪 Out' : '❌ Out Fail';
    
    console.log(`[${i + 1}/${roomList.length}] ${joinIcon} Join | ${pkIcon} | ${exitIcon} -> Room: ${room.roomId} | "${room.topic}"`);

    results.push({
      STT: i + 1,
      'Room ID': room.roomId,
      'Tên Phòng (Topic)': room.topic,
      'Thể Loại': room.classifyCode,
      'Join Room': joinRes.success ? 'Thành công' : 'Thất bại',
      'Mở PK': pkRes.success ? 'Thành công' : (pkRes.message || (pkRes.code ? `Code ${pkRes.code}` : 'Thất bại')),
      'Thoát Phòng': exitRes.success ? 'Thành công' : 'Thất bại'
    });

    if (i < roomList.length - 1 && delayMs > 0) {
      await new Promise(r => setTimeout(r, delayMs));
    }
  }

  console.log('\n================ BẢNG TỔNG KẾT KẾT QUẢ [JOIN ➔ PK ➔ THOÁT] ================');
  console.table(results);
  console.log('===========================================================================\n');

  return results;
}

/**
 * Cấu hình chu kỳ lặp lại (Cron Loop)
 */
const LOOP_CONFIG = {
  // Khoảng thời gian nghỉ giữa các vòng quét (mặc định 60 giây)
  intervalSeconds: 60,
  // Delay giữa mỗi lần gọi API trong một phòng (ms)
  delayBetweenRoomsMs: 300
};

/**
 * Hàm khởi chạy chính - Chạy vòng lặp vô tận (Infinite Cron Loop)
 */
async function main() {
  console.log('========================================================================================');
  console.log('       🔥 POPUP GLOBAL BOT - AUTO SCAN, CATEGORIZE, JOIN, APPLY PK & EXIT LOOP');
  console.log('========================================================================================\n');

  let round = 1;

  while (true) {
    const startTime = new Date();
    console.log(`\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>`);
    console.log(`[⏱️ ${startTime.toLocaleTimeString()}] BẮT ĐẦU VÒNG LẶP #${round}`);
    console.log(`<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n`);

    try {
      // 1. Đăng nhập lấy Token mới nhất cho từng vòng
      const token = await loginAndGetToken();

      // 2. Quét tất cả Recommend
      const recommendRooms = await scanAllRecommendRooms(token);

      // 3. Quét tất cả Hot
      const hotRooms = await scanHotRooms(token);

      // 4. Quét tất cả Classify
      const classifyRooms = await scanClassifyRooms(token);

      // 5. Gom tất cả nguồn lại
      const rawAllRooms = [...recommendRooms, ...hotRooms, ...classifyRooms];

      // 6. Phân tích, gom nhóm và hiển thị bảng theo thể loại
      const { uniqueRooms } = aggregateAndDisplayCategories(rawAllRooms);

      // 7. Tự động thực hiện chuỗi: Join -> Mở PK -> Thoát phòng
      if (uniqueRooms.length > 0) {
        await processRoomPipeline(token, uniqueRooms, LOOP_CONFIG.delayBetweenRoomsMs);
      } else {
        console.log('[-] Không tìm thấy phòng nào trong vòng này.');
      }

    } catch (error) {
      console.error(`[-] Đã xảy ra lỗi ở Vòng #${round}:`, error.message);
      console.log('[!] Hệ thống sẽ tự động phục hồi và tiếp tục ở vòng tiếp theo...');
    }

    round++;
    console.log(`\n[☕] Hoàn tất vòng #${round - 1}. Nghỉ ${LOOP_CONFIG.intervalSeconds}s trước khi bắt đầu vòng #${round}...`);

    // Đếm ngược thời gian nghỉ
    for (let s = LOOP_CONFIG.intervalSeconds; s > 0; s--) {
      process.stdout.write(`    ⏳ Đang chờ vòng tiếp theo: ${s}s...\r`);
      await new Promise(r => setTimeout(r, 1000));
    }
    console.log('\n');
  }
}

main();


