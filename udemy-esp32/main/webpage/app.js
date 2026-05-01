

/**
 * ═══════════════════════════════════════════════════════
 *  HAND CONTROL — state
 * ═══════════════════════════════════════════════════════
 */
var handInitialized  = false;
var handRenderer, handScene, handCamera, handGroup;
var handKeyLight, handRimLight;
var handFingerObjects = {};
var handOrb      = { theta: 0.28, phi: 1.22, radius: 8.2 };
var handDragging = false;
var handPrevPt   = { x: 0, y: 0 };
var handPrevTouch = null;
var handLastT    = 0;

var HAND_NAMES = ['Thumb', 'Index', 'Middle', 'Ring', 'Pinky'];
var handCurAng = { Thumb:180, Index:180, Middle:180, Ring:180, Pinky:180 };
var handTgtAng = { Thumb:180, Index:180, Middle:180, Ring:180, Pinky:180 };


/**
 * Initialize hand 
 */
$(window).on('load', function() {
	initHand();
});

/**
 * Lazy-initialise the Three.js hand scene.
 * Called the first time the Hand Control tab is shown.
 */
function initHand() {
	var vp     = document.getElementById('hand-viewport');
	var canvas = document.getElementById('hand-canvas');

	if (handInitialized) {
		// Re-fit canvas to container if tab was hidden during a resize
		handRenderer.setSize(vp.clientWidth, vp.clientHeight);
		handCamera.aspect = vp.clientWidth / vp.clientHeight;
		handCamera.updateProjectionMatrix();
		return;
	}

	if (typeof THREE === 'undefined') {
		vp.innerHTML = '<p style="color:#e55;padding:20px;text-align:center;">' +
		               'Three.js failed to load.<br>Check internet connection.</p>';
		return;
	}

	handInitialized = true;

	// ── Renderer ────────────────────────────────────────
	handRenderer = new THREE.WebGLRenderer({ canvas: canvas, antialias: true });
	handRenderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
	handRenderer.shadowMap.enabled = true;
	handRenderer.shadowMap.type    = THREE.PCFSoftShadowMap;
	handRenderer.setClearColor(0x0a1428);
	handRenderer.outputEncoding    = THREE.sRGBEncoding;
	handRenderer.setSize(vp.clientWidth, vp.clientHeight);

	// ── Scene & Camera ──────────────────────────────────
	handScene  = new THREE.Scene();
	handCamera = new THREE.PerspectiveCamera(44, vp.clientWidth / vp.clientHeight, 0.1, 200);

	window.addEventListener('resize', function () {
		if (!vp || vp.style.display === 'none') { return; }
		handRenderer.setSize(vp.clientWidth, vp.clientHeight);
		handCamera.aspect = vp.clientWidth / vp.clientHeight;
		handCamera.updateProjectionMatrix();
	});

	// ── Lighting ─────────────────────────────────────────
	handScene.add(new THREE.AmbientLight(0x8899bb, 1.2));

	var sun = new THREE.DirectionalLight(0xffffff, 2.0);
	sun.position.set(5, 10, 8);
	sun.castShadow = true;
	sun.shadow.mapSize.width = sun.shadow.mapSize.height = 1024;
	sun.shadow.camera.left   = -5;  sun.shadow.camera.right  =  5;
	sun.shadow.camera.top    =  6;  sun.shadow.camera.bottom = -5;
	sun.shadow.camera.far    = 30;
	handScene.add(sun);

	handKeyLight = new THREE.PointLight(0x6699ff, 3.5, 20);
	handKeyLight.position.set(-3, 3, 4);
	handScene.add(handKeyLight);

	handRimLight = new THREE.PointLight(0xff8844, 2.0, 15);
	handRimLight.position.set(3, -1, 3);
	handScene.add(handRimLight);

	var topLight = new THREE.PointLight(0xaaccff, 1.5, 18);
	topLight.position.set(0, 6, -2);
	handScene.add(topLight);

	// ── Materials ────────────────────────────────────────
	function hMat(hex, m, r) {
		return new THREE.MeshStandardMaterial({ color: hex, metalness: m, roughness: r });
	}
	var mPalm  = hMat(0x2a5c8c, 0.45, 0.35);
	var mWrist = hMat(0x1e4870, 0.40, 0.40);
	var mSeg   = [hMat(0x3a7ab8, 0.40, 0.30),
	              hMat(0x2e679e, 0.45, 0.30),
	              hMat(0x225285, 0.50, 0.28)];
	var mJoint = hMat(0x6aaee0, 0.55, 0.20);
	mJoint.emissive           = new THREE.Color(0x1a3a60);
	mJoint.emissiveIntensity  = 0.3;
	var mPlat  = hMat(0x0d1e3c, 0.60, 0.50);
	var mRing  = new THREE.MeshStandardMaterial({
		color: 0x2060ee, emissive: 0x1030bb, metalness: 0.8, roughness: 0.1
	});

	// ── Geometry helpers ─────────────────────────────────
	function hBox(w, h, d, mat) {
		var m = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat);
		m.castShadow = m.receiveShadow = true;
		return m;
	}
	function hSph(r, mat) {
		var m = new THREE.Mesh(new THREE.SphereGeometry(r, 12, 12), mat);
		m.castShadow = true;
		return m;
	}
	function hCyl(rt, rb, h, mat) {
		var m = new THREE.Mesh(new THREE.CylinderGeometry(rt, rb, h, 18), mat);
		m.castShadow = m.receiveShadow = true;
		return m;
	}

	// ── Platform ─────────────────────────────────────────
	var plat = hCyl(2.6, 2.9, 0.14, mPlat);
	plat.position.y = -2.3;
	handScene.add(plat);

	var ringMesh = new THREE.Mesh(new THREE.TorusGeometry(2.5, 0.03, 8, 80), mRing);
	ringMesh.rotation.x = Math.PI / 2;
	ringMesh.position.y = -2.23;
	handScene.add(ringMesh);

	// Background particles
	var pN = 250, pPos = new Float32Array(pN * 3);
	for (var pi = 0; pi < pN; pi++) {
		pPos[pi*3]   = (Math.random() - 0.5) * 24;
		pPos[pi*3+1] = (Math.random() - 0.5) * 18;
		pPos[pi*3+2] = (Math.random() - 0.5) * 24 - 5;
	}
	var pGeo = new THREE.BufferGeometry();
	pGeo.setAttribute('position', new THREE.BufferAttribute(pPos, 3));
	handScene.add(new THREE.Points(pGeo, new THREE.PointsMaterial({ color: 0x2a5090, size: 0.06 })));

	// ── Hand group ───────────────────────────────────────
	handGroup = new THREE.Group();
	handScene.add(handGroup);

	handGroup.add(hBox(1.55, 1.95, 0.33, mPalm));

	var kxs = [-0.52, -0.19, 0.14, 0.47];
	for (var ki = 0; ki < 4; ki++) {
		var kn = hSph(0.105, mJoint);
		kn.position.set(kxs[ki], 0.95, 0.12);
		kn.scale.set(1, 0.65, 0.85);
		handGroup.add(kn);
	}

	var wristMesh = hCyl(0.40, 0.47, 1.1, mWrist);
	wristMesh.position.y = -1.52;
	handGroup.add(wristMesh);

	var forearm = hCyl(0.30, 0.40, 0.55, mWrist);
	forearm.position.y = -2.28;
	handGroup.add(forearm);

	// ── Finger factory ───────────────────────────────────
	function buildFinger(lengths, baseW) {
		var pivots = [];

		function seg(len, ws, mi) {
			var g = new THREE.Group(), bw = baseW * ws;
			var b = hBox(bw, len, bw * 0.88, mSeg[mi]);
			b.position.y = len / 2;
			g.add(b);
			g.add(hSph(bw * 0.56, mJoint));
			return g;
		}

		var p0 = new THREE.Group();
		p0.add(seg(lengths[0], 1.00, 0));

		var p1 = new THREE.Group();
		p1.position.y = lengths[0];
		p0.add(p1);
		p1.add(seg(lengths[1], 0.87, 1));

		var p2 = new THREE.Group();
		p2.position.y = lengths[1];
		p1.add(p2);
		p2.add(seg(lengths[2], 0.74, 2));

		var tip = hSph(baseW * 0.74 * 0.46, mSeg[2]);
		tip.position.y = lengths[2];
		tip.scale.set(1, 1.1, 0.9);
		p2.add(tip);

		pivots.push(p0, p1, p2);
		return { rootPivot: p0, pivots: pivots };
	}

	// 4 regular fingers
	var fdefs = [
		['Index',   0.40,  0.975, [0.53, 0.40, 0.29], 0.195],
		['Middle',  0.11,  0.975, [0.59, 0.44, 0.31], 0.206],
		['Ring',   -0.19,  0.975, [0.53, 0.40, 0.29], 0.195],
		['Pinky',  -0.46,  0.840, [0.41, 0.29, 0.21], 0.155]
	];
	for (var fi = 0; fi < fdefs.length; fi++) {
		var fd = fdefs[fi];
		var f  = buildFinger(fd[3], fd[4]);
		f.rootPivot.position.set(fd[1], fd[2], 0.06);
		handGroup.add(f.rootPivot);
		handFingerObjects[fd[0]] = f;
	}

	// Thumb (2 segments, angled out)
	var tBase = new THREE.Group();
	tBase.position.set(0.75, 0.30, 0.05);
	tBase.rotation.z = -Math.PI / 3.8;
	handGroup.add(tBase);

	var tp0 = new THREE.Group();
	tBase.add(tp0);
	var ts0 = hBox(0.22, 0.42, 0.20, mSeg[0]);
	ts0.position.y = 0.21;
	tp0.add(ts0);
	tp0.add(hSph(0.127, mJoint));

	var tp1 = new THREE.Group();
	tp1.position.y = 0.42;
	tp0.add(tp1);
	var ts1 = hBox(0.19, 0.33, 0.17, mSeg[1]);
	ts1.position.y = 0.165;
	tp1.add(ts1);
	tp1.add(hSph(0.110, mJoint));

	var tTip = hSph(0.093, mSeg[2]);
	tTip.position.y = 0.33;
	tTip.scale.set(1, 1.1, 0.9);
	tp1.add(tTip);

	handFingerObjects['Thumb'] = { rootPivot: tBase, pivots: [tp0, tp1], isThumb: true };

	// ── Camera position ──────────────────────────────────
	handApplyOrbit();

	// ── Orbit controls ───────────────────────────────────
	canvas.addEventListener('mousedown', function (e) {
		handDragging = true;
		handPrevPt = { x: e.clientX, y: e.clientY };
	});
	window.addEventListener('mouseup', function () { handDragging = false; });
	window.addEventListener('mousemove', function (e) {
		if (!handDragging) { return; }
		handOrb.theta -= (e.clientX - handPrevPt.x) * 0.008;
		handOrb.phi    = Math.max(0.22, Math.min(2.65,
		                 handOrb.phi - (e.clientY - handPrevPt.y) * 0.008));
		handPrevPt = { x: e.clientX, y: e.clientY };
		handApplyOrbit();
	});
	canvas.addEventListener('wheel', function (e) {
		handOrb.radius = Math.max(4, Math.min(16, handOrb.radius + e.deltaY * 0.012));
		handApplyOrbit();
		e.preventDefault();
	}, { passive: false });

	canvas.addEventListener('touchstart', function (e) {
		handPrevTouch = { x: e.touches[0].clientX, y: e.touches[0].clientY };
	}, { passive: true });
	canvas.addEventListener('touchmove', function (e) {
		if (!handPrevTouch) { return; }
		handOrb.theta -= (e.touches[0].clientX - handPrevTouch.x) * 0.01;
		handOrb.phi    = Math.max(0.22, Math.min(2.65,
		                 handOrb.phi - (e.touches[0].clientY - handPrevTouch.y) * 0.01));
		handPrevTouch = { x: e.touches[0].clientX, y: e.touches[0].clientY };
		handApplyOrbit();
		e.preventDefault();
	}, { passive: false });

	// ── Build UI cards ───────────────────────────────────
	var cardsEl = document.getElementById('hand-finger-cards');
	for (var ci = 0; ci < HAND_NAMES.length; ci++) {
		(function (nm) {
			var card = document.createElement('div');
			card.className = 'hfinger-card';
			card.innerHTML =
				'<div class="hcard-header">' +
					'<span class="hfinger-label">' + nm + '</span>' +
					'<span class="hangle-badge" id="hbadge-' + nm + '">180°</span>' +
				'</div>' +
				'<div class="hangle-bar">' +
					'<div class="hangle-fill" id="hfill-' + nm + '" style="width:100%"></div>' +
				'</div>' +
				'<div class="hcard-btns">' +
					'<button class="hbtn hbtn-open"  onclick="handSetTarget(\'' + nm + '\',180)">Open</button>' +
					'<button class="hbtn hbtn-close" onclick="handSetTarget(\'' + nm + '\',0)">Close</button>' +
				'</div>';
			cardsEl.appendChild(card);
		})(HAND_NAMES[ci]);
	}

	// Initialise all fingers to open
	for (var ni = 0; ni < HAND_NAMES.length; ni++) {
		applyHandAngle(HAND_NAMES[ni], 180);
	}

	requestAnimationFrame(handAnimate);
}

// ── Camera orbit ─────────────────────────────────────
function handApplyOrbit() {
	if (!handCamera) { return; }
	var t = handOrb.theta, p = handOrb.phi, r = handOrb.radius;
	handCamera.position.set(
		r * Math.sin(p) * Math.sin(t),
		r * Math.cos(p) + 0.5,
		r * Math.sin(p) * Math.cos(t)
	);
	handCamera.lookAt(0, 0.25, 0);
}

// ── Apply joint rotations for a given angle ──────────
function applyHandAngle(name, deg) {
	var f = handFingerObjects[name];
	if (!f) { return; }
	var t = deg / 180;   // 0 = closed, 1 = open
	if (f.isThumb) {
		f.pivots[0].rotation.x = -(1 - t) * 1.0;
		f.pivots[0].rotation.z =  (1 - t) * 0.55;
		f.pivots[1].rotation.x = -(1 - t) * 0.7;
	} else {
		f.pivots[0].rotation.x = -(1 - t) * 1.52;
		f.pivots[1].rotation.x = -(1 - t) * 1.48;
		f.pivots[2].rotation.x = -(1 - t) * 1.05;
	}
}

/**
 * Set a finger's target angle and send it to the ESP32.
 * @param {string} name  - Finger name (Thumb/Index/Middle/Ring/Pinky)
 * @param {number} deg   - Target angle 0–180
 */
function handSetTarget(name, deg) {
	handTgtAng[name] = Math.max(0, Math.min(180, deg));
	sendAngle(name, handTgtAng[name]);
}

function handOpenAll() {
	for (var i = 0; i < HAND_NAMES.length; i++) { handSetTarget(HAND_NAMES[i], 180); }
}
function handCloseAll() {
	for (var i = 0; i < HAND_NAMES.length; i++) { handSetTarget(HAND_NAMES[i], 0); }
}


/**
 * ═══════════════════════════════════════════════════════
 *  PRESETS
 *  Each preset is { Thumb, Index, Middle, Ring, Pinky }
 *  Angles: 0 = closed (curled), 180 = open (extended)
 * ═══════════════════════════════════════════════════════
 */
var HAND_PRESETS = {
    point:  { Thumb:   0, Index: 180, Middle:   0, Ring:   0, Pinky:   0 },
    rockon: { Thumb:   180, Index: 180, Middle:   0, Ring:   0, Pinky: 180 },
    peace:  { Thumb:   0, Index: 180, Middle: 180, Ring:   0, Pinky:   0 }
};

function handPreset(name) {
    var preset = HAND_PRESETS[name];
    if (!preset) { return; }

    HAND_NAMES.forEach(function(finger) {
        handSetTarget(finger, preset[finger]);
    });
}

/**
 * POST finger angle to the ESP32 servo controller.
 * ESP32 endpoint: POST /hand/angle
 * Body: finger=<name>&angle=<0-180>
 */
function sendAngle(finger, angle) {
	var xhr = new XMLHttpRequest();
	xhr.open('POST', '/hand/angle', true);
	xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
	xhr.send('finger=' + encodeURIComponent(finger) + '&angle=' + Math.round(angle));
}

// ── Render loop ──────────────────────────────────────
function handAnimate(t) {
	requestAnimationFrame(handAnimate);
	if (!handRenderer) { return; }

	var dt = Math.min(t - handLastT, 50);
	handLastT = t;

	for (var i = 0; i < HAND_NAMES.length; i++) {
		var nm   = HAND_NAMES[i];
		var diff = handTgtAng[nm] - handCurAng[nm];
		handCurAng[nm] += diff * Math.min(1, dt * 0.006);
		applyHandAngle(nm, handCurAng[nm]);

		var badge = document.getElementById('hbadge-' + nm);
		var fill  = document.getElementById('hfill-'  + nm);
		if (badge) { badge.textContent  = Math.round(handCurAng[nm]) + '°'; }
		if (fill)  { fill.style.width   = (handCurAng[nm] / 180 * 100).toFixed(1) + '%'; }
	}

	var s = t * 0.001;
	if (handGroup)    { handGroup.position.y = Math.sin(s * 0.55) * 0.04; }
	if (handKeyLight) { handKeyLight.position.set(-3 + Math.sin(s * 0.6) * 0.55, 3, 4); }
	if (handRimLight) { handRimLight.position.set( 3 + Math.cos(s * 0.4) * 0.45, -1, 3); }

	handRenderer.render(handScene, handCamera);
}


