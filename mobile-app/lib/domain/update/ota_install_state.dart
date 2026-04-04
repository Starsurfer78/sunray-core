class OtaInstallState {
  const OtaInstallState({
    this.isDownloading = false,
    this.progress,
    this.downloadedFilePath,
    this.statusMessage,
    this.errorMessage,
  });

  final bool isDownloading;
  final double? progress;
  final String? downloadedFilePath;
  final String? statusMessage;
  final String? errorMessage;

  OtaInstallState copyWith({
    bool? isDownloading,
    double? progress,
    String? downloadedFilePath,
    String? statusMessage,
    String? errorMessage,
    bool clearDownloadedFilePath = false,
    bool clearStatusMessage = false,
    bool clearErrorMessage = false,
  }) {
    return OtaInstallState(
      isDownloading: isDownloading ?? this.isDownloading,
      progress: progress ?? this.progress,
      downloadedFilePath: clearDownloadedFilePath
          ? null
          : (downloadedFilePath ?? this.downloadedFilePath),
      statusMessage: clearStatusMessage ? null : (statusMessage ?? this.statusMessage),
      errorMessage: clearErrorMessage ? null : (errorMessage ?? this.errorMessage),
    );
  }
}
