use base64::{Engine as _, engine::general_purpose};

pub fn base64_encode(data: &str) -> String {
    general_purpose::STANDARD.encode(data)
}

pub fn base64_decode(data: &str) -> Result<Vec<u8>, base64::DecodeError> {
    general_purpose::STANDARD.decode(data)
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_base64_encoding() {
        let input = "hello world";
        let encoded = base64_encode(input);
        assert_eq!(encoded, "aGVsbG8gd29ybGQ=");
        
        let decoded = base64_decode(&encoded).unwrap();
        assert_eq!(String::from_utf8(decoded).unwrap(), input);
    }
}