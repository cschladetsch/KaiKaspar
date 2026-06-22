import cv2
import numpy as np
import glob
import json

# Calibration parameters
CHECKERBOARD = (6, 9)
CRITERIA = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

def calibrate():
    # Vector to store 3D points for each checkerboard image
    objpoints = []
    # Vector to store 2D points for each checkerboard image
    imgpoints = []

    # Defining the world coordinates for 3D points
    objp = np.zeros((1, CHECKERBOARD[0] * CHECKERBOARD[1], 3), np.float32)
    objp[0, :, :2] = np.mgrid[0:CHECKERBOARD[0], 0:CHECKERBOARD[1]].T.reshape(-1, 2)

    images = glob.glob('data/calibration/*.jpg')
    if not images:
        print("No calibration images found in data/calibration/*.jpg")
        return

    gray = None
    for fname in images:
        img = cv2.imread(fname)
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        # Find the chess board corners
        ret, corners = cv2.findChessboardCorners(gray, CHECKERBOARD, cv2.CALIB_CB_ADAPTIVE_THRESH + cv2.CALIB_CB_FAST_CHECK + cv2.CALIB_CB_NORMALIZE_IMAGE)
        if ret == True:
            objpoints.append(objp)
            # Refining pixel coordinates for given 2d points.
            corners2 = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), CRITERIA)
            imgpoints.append(corners2)

    if not imgpoints:
        print("Could not find corners in any calibration images.")
        return

    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None)

    calibration_data = {
        "camera_matrix": mtx.tolist(),
        "dist_coeffs": dist.tolist(),
        "reprojection_error": ret
    }

    with open("kaicore/src/main/assets/camera_calibration.json", "w") as f:
        json.dump(calibration_data, f, indent=2)
    
    print("Calibration complete. Results saved to kaicore/src/main/assets/camera_calibration.json")

if __name__ == "__main__":
    calibrate()
