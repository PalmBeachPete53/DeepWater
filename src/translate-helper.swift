// deepwater-translate: translates a page's paragraphs on this Mac with Apple's Translation framework, for the News
// reader. JSON on stdin ({"target":"fr","texts":[…]}), JSON on stdout ({"source":"en","texts":[…]} or {"error":"…"}).
// Nothing leaves the machine: only languages already installed are used (TranslationSession(installedSource:target:)),
// so no download starts and no dialog opens from here.
import Foundation
import NaturalLanguage
import Translation

struct Request: Codable { let target: String; let texts: [String] }
struct Answer: Codable { var source: String? = nil; var texts: [String]? = nil; var error: String? = nil }

func finish(_ answer: Answer) -> Never {
    FileHandle.standardOutput.write((try? JSONEncoder().encode(answer)) ?? Data())
    exit(0)
}

guard let request = try? JSONDecoder().decode(Request.self, from: FileHandle.standardInput.readDataToEndOfFile()) else {
    finish(Answer(error: "bad_request"))
}
let recognizer = NLLanguageRecognizer()
recognizer.processString(String(request.texts.joined(separator: " ").prefix(6000)))
guard let source = recognizer.dominantLanguage?.rawValue else { finish(Answer(error: "unknown_language")) }
if source.prefix(2) == request.target.prefix(2) { finish(Answer(source: source, error: "same_language")) }

Task.detached {
    guard #available(macOS 26.0, *) else { finish(Answer(source: source, error: "system_too_old")) }
    let from = Locale.Language(identifier: source), to = Locale.Language(identifier: request.target)
    switch await LanguageAvailability().status(from: from, to: to) {
    case .installed: break
    case .supported: finish(Answer(source: source, error: "not_installed"))
    default: finish(Answer(source: source, error: "unsupported"))
    }
    do {
        let session = TranslationSession(installedSource: from, target: to)
        var out = request.texts
        let requests = request.texts.indices.map { TranslationSession.Request(sourceText: request.texts[$0], clientIdentifier: String($0)) }
        for response in try await session.translations(from: requests) {
            if let id = response.clientIdentifier, let index = Int(id), out.indices.contains(index) {
                out[index] = response.targetText
            }
        }
        finish(Answer(source: source, texts: out))
    } catch {
        finish(Answer(source: source, error: "failed"))
    }
}
dispatchMain()
