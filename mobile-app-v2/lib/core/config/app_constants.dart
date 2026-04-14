class AppConstants {
  const AppConstants._();

  static const String appDisplayName = 'Sunray Alfred';
  static const String appVersionFallback = '1.0.0';
  static const String githubRepoOwner = 'Starsurfer78';
  static const String githubRepoName = 'sunray-core';
  static const String githubReleasesApi =
      'https://api.github.com/repos/$githubRepoOwner/$githubRepoName/releases';
  static const String githubReleasesPage =
      'https://github.com/$githubRepoOwner/$githubRepoName/releases';
  static const String defaultRobotScheme = 'http';
  static const int defaultRobotPort = 8765;
  static const List<int> fallbackRobotPorts = <int>[8765, 8080];
  static const String defaultRobotHost = 'alfred.local';
}
