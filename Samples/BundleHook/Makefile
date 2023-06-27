.PHONY: all
all:
	xcodebuild build -derivedDataPath build -scheme MyApp -configuration Debug -destination 'generic/platform=iOS' CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
	xcodebuild build -derivedDataPath build -scheme MyApp -configuration Release -destination 'generic/platform=iOS' CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
